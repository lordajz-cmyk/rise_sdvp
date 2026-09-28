//! statusskarm — fullskärmsstatus på robotens egen Pi (5" touchskärm i skåpet).
//!
//! Vid start: en stor nedräkning, 3:00 (modemet tar ~90 s, resten är uppe inom
//! 3 min). Tryck var som helst för att hoppa förbi. Sedan sju rutor som uppdateras
//! var 2:a sekund. Grön = OK, grå = inte OK. CPU-rutan är grön/gul/röd.
//!
//! Tryck på Car_Client-, Internet- eller RTK-rutan när den inte är grön, eller på Pi-rutan
//! (alltid), för att starta om efter en Ja/Nej-fråga. Kommandona körs med
//! `sudo -n` och är begränsade av en sudoers-regel (se installera.sh).
//!
//! Allt läses lokalt på Pi:n och ingenting här tar Car_Client:s TCP-plats (8300),
//! så robotd/RControlStation påverkas inte:
//! - styrkortet: UDP till Car_Client (8300 → svar på 8301), kommandot
//!   CMD_AP_GET_ROUTE_PART med 0 punkter. Det ändrar inget och nollställer INTE
//!   styrkortets säkerhetstid (CMD_GET_STATE skulle göra det, och då skulle
//!   kortet aldrig stoppa maskinen om RControlStation-länken dör).
//! - RTK Float/Fix: NMEA GGA från rtkrcv på TCP 2948 (tål flera lyssnare).
//! - WireGuard: senaste handskakning (`wg show wg0 latest-handshakes`).
//!
//!   statusskarm            helskärm (på Pi:n)
//!   statusskarm --demo     påhittad status, omstarter bara låtsas (för test på datorn)
//!   statusskarm --fonster  fönster 720x1280 i stället för helskärm
//!   statusskarm --fonster=800x480   fönster i valfri storlek (t.ex. en 4,3"-skärm)
//!   statusskarm --utan-nedrakning   visa rutorna direkt
//!   statusskarm --text     ingen skärm: skriv status i terminalen var 2:a sekund (felsökning)

use std::io::{BufRead, BufReader};
use std::net::{TcpStream, UdpSocket};
use std::process::Command;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use eframe::egui;

const COUNTDOWN: Duration = Duration::from_secs(180);
const POLL: Duration = Duration::from_secs(2);
/// Styrkortet frågas mer sällan: Car_Client skriver en loggrad per svar.
const BOARD_POLL: Duration = Duration::from_secs(4);
/// Efter en omstart: rutan visar "Startar om..." och går inte att trycka på så länge.
const RESTART_LOCK: Duration = Duration::from_secs(10);
/// WireGuard räknas som uppe om senaste handskakning är yngre än så (servern
/// håller liv i tunneln var 25:e s, en ny handskakning sker varannan minut).
const HANDSHAKE_MAX_AGE: u64 = 180;
/// En GGA äldre än så räknas som ingen position.
const GGA_MAX_AGE: Duration = Duration::from_secs(5);

const CAR_CLIENT_ADDR: &str = "127.0.0.1:8300";
const BOARD_REPLY_ADDR: &str = "127.0.0.1:8301";
/// Unix-tid (s) för styrkortets senaste svar, skrivs av den instans som äger port
/// 8301 och läses av en andra instans (t.ex. `--text` över ssh).
const BOARD_SHARED_FILE: &str = "/tmp/statusskarm_styrkort_svar";
const RTKRCV_NMEA_ADDR: &str = "127.0.0.1:2948";
const USB_DEVICE: &str = "/dev/vehicle";
/// ID 255 = alla, så kortet svarar oavsett vilket bil-ID Car_Client kör med.
const CMD_AP_GET_ROUTE_PART: u8 = 58;
const BOARD_QUERY: [u8; 7] = [255, CMD_AP_GET_ROUTE_PART, 0, 0, 0, 0, 0];

// Färger: mörk bakgrund, rutor lite ljusare i vila.
const BG: egui::Color32 = egui::Color32::from_rgb(18, 20, 24);
const BOX_IDLE: egui::Color32 = egui::Color32::from_rgb(52, 58, 68);
const BOX_OK: egui::Color32 = egui::Color32::from_rgb(40, 150, 70);
const BOX_WARN: egui::Color32 = egui::Color32::from_rgb(214, 160, 0);
const BOX_BAD: egui::Color32 = egui::Color32::from_rgb(190, 50, 40);
const TEXT: egui::Color32 = egui::Color32::WHITE;
const TEXT_DIM: egui::Color32 = egui::Color32::from_rgb(200, 205, 212);

/// Senast kända status, skrivs av insamlingstråden och läses av gränssnittet.
#[derive(Clone, Default)]
struct Status {
    usb_present: bool,
    board_ok: bool,
    car_client_ok: bool,
    /// Sekunder sedan senaste WireGuard-handskakning.
    handshake_age: Option<u64>,
    /// Kunde inte läsa handskakningen (t.ex. sudoers-regeln saknas).
    handshake_error: bool,
    rtk_service_ok: bool,
    /// GGA fix-kvalitet: 0 ingen, 1 GPS, 2 DGPS, 4 RTK Fix, 5 RTK Float.
    fix_quality: Option<u8>,
    cpu_percent: Option<f32>,
    /// När CPU:n senast gick över 90 % (för "röd efter 10 s").
    cpu_very_high_since: Option<Instant>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Action {
    RestartCarClient,
    RestartRtk,
    Reboot,
    RestartWireGuard,
}

impl Action {
    fn question(self) -> &'static str {
        match self {
            Action::RestartCarClient => "Är du säker att du vill starta om car_service?",
            Action::RestartRtk => "Är du säker att du vill starta om RTK-tjänsten (car_rtk)?",
            Action::Reboot => "Är du säker att du vill starta om Pi:n?",
            Action::RestartWireGuard => "Är du säker att du vill starta om internet (WireGuard)?",
        }
    }

    fn warning(self) -> Option<&'static str> {
        match self {
            Action::Reboot => Some(
                "Allt stängs ner tills Pi:n är uppe igen: Car_Client, RTK och \
                 WireGuard-tunneln. Alla fjärranslutningar bryts. \
                 Därefter visas nedräkningen på nytt.",
            ),
            Action::RestartWireGuard => Some(
                "Tunneln tas ner och upp igen. Fjärranslutningar (RControlStation, ssh) \
                 bryts en kort stund och får anslutas på nytt.",
            ),
            _ => None,
        }
    }

    fn command(self) -> &'static [&'static str] {
        match self {
            Action::RestartCarClient => &["sudo", "-n", "/usr/bin/systemctl", "restart", "car_client.service"],
            Action::RestartRtk => &["sudo", "-n", "/usr/bin/systemctl", "restart", "car_rtk.service"],
            Action::Reboot => &["sudo", "-n", "/usr/bin/systemctl", "reboot"],
            // Samma som wg-quick down + up, men via systemd som äger tunneln, så att
            // wg-quick@wg0 inte står som aktiv när tunneln i själva verket är nere.
            Action::RestartWireGuard => &["sudo", "-n", "/usr/bin/systemctl", "restart", "wg-quick@wg0.service"],
        }
    }

    fn index(self) -> usize {
        self as usize
    }
}

fn main() -> eframe::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let demo = args.iter().any(|a| a == "--demo");
    let windowed = args.iter().any(|a| a == "--fonster" || a.starts_with("--fonster="));
    let window_size = args
        .iter()
        .find_map(|a| a.strip_prefix("--fonster="))
        .and_then(|s| s.split_once('x'))
        .and_then(|(w, h)| Some([w.parse::<f32>().ok()?, h.parse::<f32>().ok()?]))
        .unwrap_or([720.0, 1280.0]);
    let skip_countdown = args.iter().any(|a| a == "--utan-nedrakning");

    let status = Arc::new(Mutex::new(Status::default()));
    if demo {
        spawn_demo_collector(status.clone());
    } else {
        spawn_collector(status.clone());
        spawn_gga_reader(status.clone());
    }

    if args.iter().any(|a| a == "--text") {
        // Samma rutor som på skärmen, fast som text: för att kontrollera signalerna
        // över ssh på en Pi utan skärm.
        let app = App::new(status, demo);
        loop {
            thread::sleep(POLL);
            let line: Vec<String> = app
                .tiles()
                .iter()
                .map(|t| {
                    let mark = if t.color == BOX_OK { "GRÖN" } else if t.color == BOX_IDLE { "grå " } else if t.color == BOX_WARN { "GUL " } else { "RÖD " };
                    format!("[{mark}] {}: {}{}", t.title, t.big.clone().map(|b| format!("{b} ")).unwrap_or_default(), t.detail)
                })
                .collect();
            use std::io::Write;
            if writeln!(std::io::stdout(), "{}", line.join("  |  ")).is_err() {
                return Ok(()); // utdata stängd (t.ex. `| head`)
            }
        }
    }

    let viewport = if windowed {
        egui::ViewportBuilder::default().with_inner_size(window_size).with_title("Status")
    } else {
        egui::ViewportBuilder::default().with_fullscreen(true).with_title("Status")
    };
    eframe::run_native(
        "statusskarm",
        eframe::NativeOptions { viewport, ..Default::default() },
        Box::new(move |_cc| {
            let mut app = App::new(status, demo);
            app.countdown_skipped = skip_countdown;
            // Bara i demoläge: öppna en bekräftelseruta direkt, för att se hur den ser ut.
            if demo {
                app.dialog = match std::env::var("STATUSSKARM_DEMO_DIALOG").as_deref() {
                    Ok("car_client") => Some(Action::RestartCarClient),
                    Ok("rtk") => Some(Action::RestartRtk),
                    Ok("pi") => Some(Action::Reboot),
                    Ok("wireguard") => Some(Action::RestartWireGuard),
                    _ => None,
                };
            }
            Box::new(app)
        }),
    )
}

// ---------------------------------------------------------------------------
// Insamling (bakgrundstrådar)

fn service_active(name: &str) -> bool {
    Command::new("systemctl")
        .args(["is-active", "--quiet", name])
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

/// Sekunder sedan styrkortet senast svarade enligt den instans som äger port 8301.
fn shared_board_reply_age() -> Option<u64> {
    let t: u64 = std::fs::read_to_string(BOARD_SHARED_FILE).ok()?.trim().parse().ok()?;
    let now = SystemTime::now().duration_since(UNIX_EPOCH).ok()?.as_secs();
    Some(now.saturating_sub(t))
}

/// Sekunder sedan senaste handskakning med någon peer på wg0.
fn wireguard_handshake_age() -> Result<Option<u64>, ()> {
    let out = Command::new("sudo")
        .args(["-n", "/usr/bin/wg", "show", "wg0", "latest-handshakes"])
        .output()
        .map_err(|_| ())?;
    if !out.status.success() {
        return Err(());
    }
    let now = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    let newest = String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter_map(|l| l.split_whitespace().nth(1)?.parse::<u64>().ok())
        .filter(|&t| t > 0)
        .max();
    Ok(newest.map(|t| now.saturating_sub(t)))
}

/// CPU-användning i procent sedan förra anropet, från /proc/stat.
struct CpuMeter {
    last: Option<(u64, u64)>,
    samples: Vec<f32>,
}

impl CpuMeter {
    fn read() -> Option<(u64, u64)> {
        let s = std::fs::read_to_string("/proc/stat").ok()?;
        let v: Vec<u64> = s.lines().next()?.split_whitespace().skip(1).filter_map(|x| x.parse().ok()).collect();
        let idle = v.get(3)? + v.get(4).unwrap_or(&0);
        Some((v.iter().sum(), idle))
    }

    /// Medel över de senaste ~6 s (tre mätningar med 2 s mellanrum).
    fn sample(&mut self) -> Option<f32> {
        let now = Self::read()?;
        let result = self.last.and_then(|(t0, i0)| {
            let dt = now.0.saturating_sub(t0);
            (dt > 0).then(|| 100.0 * (1.0 - now.1.saturating_sub(i0) as f32 / dt as f32))
        });
        self.last = Some(now);
        if let Some(p) = result {
            self.samples.push(p.clamp(0.0, 100.0));
            if self.samples.len() > 3 {
                self.samples.remove(0);
            }
        }
        (!self.samples.is_empty()).then(|| self.samples.iter().sum::<f32>() / self.samples.len() as f32)
    }
}

fn spawn_collector(status: Arc<Mutex<Status>>) {
    thread::spawn(move || {
        let mut cpu = CpuMeter { last: None, samples: Vec::new() };
        // Svaren från styrkortet kommer till den adress som senast skickade UDP
        // till Car_Client, port 8300 + 1.
        let socket = UdpSocket::bind(BOARD_REPLY_ADDR).ok();
        if let Some(s) = &socket {
            let _ = s.set_read_timeout(Some(Duration::from_millis(1200)));
        }
        let mut last_board_query: Option<Instant> = None;
        let mut last_board_reply: Option<Instant> = None;

        loop {
            let started = Instant::now();

            let usb_present = std::path::Path::new(USB_DEVICE).exists();
            if let Some(s) = &socket {
                if last_board_query.map_or(true, |t| t.elapsed() >= BOARD_POLL) {
                    last_board_query = Some(Instant::now());
                    let mut buf = [0u8; 2048];
                    // Car_Client skickar ALLT kortet säger hit (GPS-text, tillståndssvar
                    // till RControlStation, ~25 paket/s). Mellan frågorna fylls bufferten,
                    // och då kastades kortets svar: "svarar inte" fast kortet svarade.
                    // Töm bufferten först (ett sent svar på förra frågan räknas också).
                    if s.set_nonblocking(true).is_ok() {
                        while let Ok(n) = s.recv(&mut buf) {
                            if n >= 2 && buf[1] == CMD_AP_GET_ROUTE_PART {
                                last_board_reply = Some(Instant::now());
                            }
                        }
                        let _ = s.set_nonblocking(false);
                    }
                    let _ = s.send_to(&BOARD_QUERY, CAR_CLIENT_ADDR);
                    // Läs tills svaret kommer, hoppa över allt annat.
                    let deadline = Instant::now() + Duration::from_millis(1200);
                    while Instant::now() < deadline {
                        match s.recv(&mut buf) {
                            Ok(n) if n >= 2 && buf[1] == CMD_AP_GET_ROUTE_PART => {
                                last_board_reply = Some(Instant::now());
                                break;
                            }
                            Ok(_) => continue,
                            Err(_) => break,
                        }
                    }
                    if last_board_reply.map_or(false, |t| t.elapsed() < BOARD_POLL) {
                        let now = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
                        let _ = std::fs::write(BOARD_SHARED_FILE, now.to_string());
                    }
                }
            }
            // Porten upptagen (den grafiska statusskärmen kör redan, t.ex. vid --text
            // över ssh): använd dess senaste svar i stället för att visa "svarar inte".
            let board_ok = usb_present
                && match &socket {
                    Some(_) => last_board_reply.map_or(false, |t| t.elapsed() < BOARD_POLL * 2),
                    None => shared_board_reply_age().map_or(false, |age| age < (BOARD_POLL * 2).as_secs()),
                };

            let car_client_ok = service_active("car_client.service");
            let rtk_service_ok = service_active("car_rtk.service");
            let handshake = wireguard_handshake_age();
            let cpu_percent = cpu.sample();

            {
                let mut st = status.lock().unwrap();
                st.usb_present = usb_present;
                st.board_ok = board_ok;
                st.car_client_ok = car_client_ok;
                st.rtk_service_ok = rtk_service_ok;
                st.handshake_error = handshake.is_err();
                st.handshake_age = handshake.ok().flatten();
                st.cpu_percent = cpu_percent;
                st.cpu_very_high_since = match cpu_percent {
                    Some(p) if p > 90.0 => st.cpu_very_high_since.or(Some(Instant::now())),
                    _ => None,
                };
            }

            thread::sleep(POLL.saturating_sub(started.elapsed()));
        }
    });
}

/// Läser GGA från rtkrcv och sparar fix-kvaliteten. Återansluter själv.
fn spawn_gga_reader(status: Arc<Mutex<Status>>) {
    let last_gga: Arc<Mutex<Option<(u8, Instant)>>> = Arc::new(Mutex::new(None));
    {
        let last_gga = last_gga.clone();
        thread::spawn(move || loop {
            if let Ok(stream) = TcpStream::connect(RTKRCV_NMEA_ADDR) {
                let _ = stream.set_read_timeout(Some(Duration::from_secs(10)));
                for line in BufReader::new(stream).lines() {
                    let Ok(line) = line else { break };
                    if let Some(q) = parse_gga_quality(&line) {
                        *last_gga.lock().unwrap() = Some((q, Instant::now()));
                    }
                }
            }
            thread::sleep(Duration::from_secs(3));
        });
    }
    thread::spawn(move || loop {
        let q = last_gga.lock().unwrap().filter(|(_, t)| t.elapsed() < GGA_MAX_AGE).map(|(q, _)| q);
        status.lock().unwrap().fix_quality = q;
        thread::sleep(Duration::from_millis(500));
    });
}

/// Fix-kvaliteten (sjätte fältet) ur en $GxGGA-mening, t.ex. $GNGGA eller $GPGGA.
fn parse_gga_quality(line: &str) -> Option<u8> {
    let line = line.trim();
    let f: Vec<&str> = line.split(',').collect();
    if f.len() < 7 || !f[0].starts_with('$') || !f[0].ends_with("GGA") {
        return None;
    }
    f[6].parse().ok()
}

/// Påhittad status för att prova utseendet utan Pi.
fn spawn_demo_collector(status: Arc<Mutex<Status>>) {
    thread::spawn(move || {
        let t0 = Instant::now();
        loop {
            let t = t0.elapsed().as_secs();
            let mut st = status.lock().unwrap();
            st.usb_present = true;
            st.board_ok = t % 60 < 50;
            st.car_client_ok = t % 40 >= 10;
            st.handshake_age = Some(t % 150);
            st.handshake_error = false;
            st.rtk_service_ok = t % 90 < 70;
            st.fix_quality = Some([0u8, 1, 5, 4][((t / 10) % 4) as usize]);
            let p = 50.0 + 45.0 * ((t as f32) / 15.0).sin();
            st.cpu_percent = Some(p);
            st.cpu_very_high_since = if p > 90.0 { st.cpu_very_high_since.or(Some(Instant::now())) } else { None };
            drop(st);
            thread::sleep(POLL);
        }
    });
}

// ---------------------------------------------------------------------------
// Gränssnitt

struct App {
    status: Arc<Mutex<Status>>,
    demo: bool,
    started: Instant,
    countdown_skipped: bool,
    dialog: Option<Action>,
    /// När respektive omstart trycktes (index = Action::index()).
    restarted_at: [Option<Instant>; 4],
}

/// En ruta som ska ritas.
struct Tile {
    title: &'static str,
    detail: String,
    /// Stort värde i mitten (CPU-procent), annars None.
    big: Option<String>,
    color: egui::Color32,
    pulsing: bool,
    action: Option<Action>,
}

impl App {
    fn new(status: Arc<Mutex<Status>>, demo: bool) -> Self {
        Self {
            status,
            demo,
            started: Instant::now(),
            countdown_skipped: false,
            dialog: None,
            restarted_at: [None; 4],
        }
    }

    fn run(&mut self, action: Action) {
        self.restarted_at[action.index()] = Some(Instant::now());
        let cmd = action.command();
        if self.demo {
            println!("(demo) skulle köra: {}", cmd.join(" "));
            return;
        }
        // Vänta inte på kommandot i gränssnittet; en omstart kan ta några sekunder.
        let cmd: Vec<String> = cmd.iter().map(|s| s.to_string()).collect();
        thread::spawn(move || {
            match Command::new(&cmd[0]).args(&cmd[1..]).status() {
                Ok(s) if s.success() => {}
                Ok(s) => eprintln!("{} misslyckades: {s}", cmd.join(" ")),
                Err(e) => eprintln!("{} kunde inte köras: {e}", cmd.join(" ")),
            }
        });
    }

    fn restarting(&self, action: Action) -> bool {
        self.restarted_at[action.index()].map_or(false, |t| t.elapsed() < RESTART_LOCK)
    }

    fn tiles(&self) -> Vec<Tile> {
        let st = self.status.lock().unwrap().clone();
        let ok = |b: bool| if b { BOX_OK } else { BOX_IDLE };
        let mut tiles = Vec::new();

        tiles.push(Tile {
            title: "Styrkort",
            detail: if !st.usb_present {
                "ingen USB".into()
            } else if st.board_ok {
                "svarar".into()
            } else {
                "svarar inte".into()
            },
            big: None,
            color: ok(st.board_ok),
            pulsing: false,
            action: None,
        });

        tiles.push(self.service_tile("Car_Client", st.car_client_ok, Action::RestartCarClient));

        let internet_ok = st.handshake_age.map_or(false, |a| a < HANDSHAKE_MAX_AGE);
        let mut internet = self.service_tile("Internet", internet_ok, Action::RestartWireGuard);
        if internet_ok {
            internet.detail = format!("WireGuard {} s", st.handshake_age.unwrap_or(0));
        } else if !self.restarting(Action::RestartWireGuard) {
            let why = if st.handshake_error {
                "kan inte läsa WireGuard".to_string()
            } else {
                match st.handshake_age {
                    Some(a) => format!("tyst i {} min", a / 60),
                    None => "ingen kontakt".into(),
                }
            };
            internet.detail = format!("{why} – tryck för omstart");
        }
        tiles.push(internet);

        tiles.push(self.service_tile("RTK-tjänst", st.rtk_service_ok, Action::RestartRtk));

        let q = st.fix_quality;
        let fix = q == Some(4);
        let float_or_better = fix || q == Some(5);
        let fix_text = match q {
            None => "ingen position".to_string(),
            Some(0) => "ingen fix".into(),
            Some(1) => "vanlig GPS".into(),
            Some(2) => "DGPS".into(),
            Some(4) => "RTK Fix".into(),
            Some(5) => "RTK Float".into(),
            Some(n) => format!("kvalitet {n}"),
        };
        tiles.push(Tile { title: "RTK Float", detail: fix_text.clone(), big: None, color: ok(float_or_better), pulsing: false, action: None });
        tiles.push(Tile { title: "RTK Fix", detail: fix_text, big: None, color: ok(fix), pulsing: false, action: None });

        let reboot = self.restarting(Action::Reboot);
        let (cpu_color, cpu_big) = match st.cpu_percent {
            None => (BOX_IDLE, "–".to_string()),
            Some(p) => {
                let red = st.cpu_very_high_since.map_or(false, |t| t.elapsed() >= Duration::from_secs(10));
                let c = if red { BOX_BAD } else if p > 70.0 { BOX_WARN } else { BOX_OK };
                (c, format!("{p:.0} %"))
            }
        };
        tiles.push(Tile {
            title: "Pi",
            detail: if reboot { "Startar om...".into() } else { "tryck för omstart".into() },
            big: Some(cpu_big),
            color: if reboot { BOX_WARN } else { cpu_color },
            pulsing: reboot,
            action: (!reboot).then_some(Action::Reboot),
        });
        tiles
    }

    /// Ruta för en tjänst som går att starta om när den inte är grön.
    fn service_tile(&self, title: &'static str, running: bool, action: Action) -> Tile {
        if self.restarting(action) {
            return Tile { title, detail: "Startar om...".into(), big: None, color: BOX_WARN, pulsing: true, action: None };
        }
        Tile {
            title,
            detail: if running { "kör".into() } else { "kör inte – tryck för omstart".into() },
            big: None,
            color: if running { BOX_OK } else { BOX_IDLE },
            pulsing: false,
            action: (!running).then_some(action),
        }
    }

    fn draw_countdown(&mut self, ui: &mut egui::Ui) {
        let rect = ui.max_rect();
        let left = COUNTDOWN.saturating_sub(self.started.elapsed()).as_secs();
        let text = format!("{}:{:02}", left / 60, left % 60);
        let size = (rect.width() * 0.3).min(rect.height() * 0.3);
        ui.painter().text(rect.center(), egui::Align2::CENTER_CENTER, text, egui::FontId::proportional(size), TEXT);
        // Tryck var som helst för att hoppa över.
        if ui.interact(rect, egui::Id::new("countdown"), egui::Sense::click()).clicked() {
            self.countdown_skipped = true;
        }
    }

    fn draw_tiles(&mut self, ui: &mut egui::Ui) -> bool {
        let tiles = self.tiles();
        let rect = ui.max_rect().shrink(12.0);
        let cols = if rect.width() > rect.height() { 4 } else { 2 };
        let rows = (tiles.len() + cols - 1) / cols;
        let gap = 12.0;
        let w = (rect.width() - gap * (cols as f32 - 1.0)) / cols as f32;
        let h = (rect.height() - gap * (rows as f32 - 1.0)) / rows as f32;
        let t = self.started.elapsed().as_secs_f32();
        let mut any_pulsing = false;

        for (i, tile) in tiles.iter().enumerate() {
            let (c, r) = (i % cols, i / cols);
            let min = rect.min + egui::vec2(c as f32 * (w + gap), r as f32 * (h + gap));
            // Sista rutan (Pi) fyller resten av sista raden i stället för att lämna ett hål.
            let span = if i + 1 == tiles.len() { cols - c } else { 1 };
            let tw = w * span as f32 + gap * (span as f32 - 1.0);
            let b = egui::Rect::from_min_size(min, egui::vec2(tw, h));
            let mut fill = tile.color;
            if tile.pulsing {
                any_pulsing = true;
                let k = 0.65 + 0.35 * (t * 4.0).sin().abs();
                fill = egui::Color32::from_rgb(
                    (fill.r() as f32 * k) as u8,
                    (fill.g() as f32 * k) as u8,
                    (fill.b() as f32 * k) as u8,
                );
            }
            ui.painter().rect_filled(b, 16.0, fill);

            // Stor text även på en liten skärm (4,3" 800x480): detaljraden radbryts
            // inom rutan i stället för att krympa.
            let title_size = (h * 0.19).min(w * 0.16);
            let detail_size = title_size * 0.68;
            let wrap = tw * 0.9;
            let painter = ui.painter();
            let centered = |text: &str, size: f32, color: egui::Color32, y: f32| {
                let mut job = egui::text::LayoutJob::simple(text.to_string(), egui::FontId::proportional(size), color, wrap);
                job.halign = egui::Align::Center; // varje radbruten rad centreras
                let galley = painter.layout_job(job);
                let pos = egui::pos2(b.center().x, y - galley.size().y / 2.0);
                painter.galley(pos, galley, color);
            };
            if let Some(big) = &tile.big {
                centered(tile.title, title_size, TEXT, b.top() + h * 0.2);
                centered(big, title_size * 1.6, TEXT, b.center().y);
                centered(&tile.detail, detail_size, TEXT_DIM, b.bottom() - h * 0.18);
            } else {
                centered(tile.title, title_size, TEXT, b.center().y - h * 0.13);
                centered(&tile.detail, detail_size, TEXT_DIM, b.center().y + h * 0.17);
            }

            if let Some(action) = tile.action {
                let resp = ui.interact(b, egui::Id::new(("tile", i)), egui::Sense::click());
                if resp.clicked() && self.dialog.is_none() {
                    self.dialog = Some(action);
                }
            }
        }
        any_pulsing
    }

    /// Ja/Nej-dialogen, samma för alla omstarter. Tryck utanför = Nej.
    fn draw_dialog(&mut self, ctx: &egui::Context) {
        let Some(action) = self.dialog else { return };
        let screen = ctx.screen_rect();

        let mut close = false;
        let mut confirmed = false;
        egui::Area::new(egui::Id::new("dialog_bg"))
            .order(egui::Order::Foreground)
            .fixed_pos(screen.min)
            .show(ctx, |ui| {
                ui.painter().rect_filled(screen, 0.0, egui::Color32::from_black_alpha(190));
                if ui.interact(screen, egui::Id::new("dialog_bg_click"), egui::Sense::click()).clicked() {
                    close = true;
                }
            });

        let w = (screen.width() * 0.86).min(700.0);
        let big = (w * 0.06).max(22.0);
        egui::Area::new(egui::Id::new("dialog"))
            .order(egui::Order::Tooltip)
            .anchor(egui::Align2::CENTER_CENTER, egui::vec2(0.0, 0.0))
            .show(ctx, |ui| {
                egui::Frame::none()
                    .fill(egui::Color32::from_rgb(38, 42, 50))
                    .rounding(18.0)
                    .inner_margin(egui::Margin::same(28.0))
                    .show(ui, |ui| {
                        ui.set_width(w);
                        ui.vertical_centered(|ui| {
                            ui.label(egui::RichText::new(action.question()).size(big).color(TEXT).strong());
                            if let Some(warn) = action.warning() {
                                ui.add_space(14.0);
                                ui.label(egui::RichText::new(warn).size(big * 0.7).color(egui::Color32::from_rgb(255, 190, 90)));
                            }
                            ui.add_space(26.0);
                            ui.horizontal(|ui| {
                                let bw = (w - 20.0) / 2.0;
                                let btn = |txt: &str, fill| {
                                    egui::Button::new(egui::RichText::new(txt).size(big).color(TEXT).strong())
                                        .fill(fill)
                                        .rounding(12.0)
                                        .min_size(egui::vec2(bw, big * 2.4))
                                };
                                if ui.add(btn("Ja", BOX_BAD)).clicked() {
                                    confirmed = true;
                                }
                                if ui.add(btn("Nej", BOX_IDLE)).clicked() {
                                    close = true;
                                }
                            });
                        });
                    });
            });

        if confirmed {
            self.dialog = None;
            self.run(action);
        } else if close {
            self.dialog = None;
        }
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let in_countdown = !self.countdown_skipped && self.started.elapsed() < COUNTDOWN;
        let mut pulsing = false;
        egui::CentralPanel::default()
            .frame(egui::Frame::none().fill(BG))
            .show(ctx, |ui| {
                if in_countdown {
                    self.draw_countdown(ui);
                } else {
                    pulsing = self.draw_tiles(ui);
                }
            });
        self.draw_dialog(ctx);

        // Rita om så sällan som möjligt: status ändras var 2:a sekund.
        let next = if pulsing || self.dialog.is_some() {
            Duration::from_millis(80)
        } else if in_countdown {
            Duration::from_millis(250)
        } else {
            Duration::from_millis(500)
        };
        ctx.request_repaint_after(next);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gga_kvalitet_tolkas() {
        assert_eq!(parse_gga_quality("$GNGGA,081530.00,6003.79,N,01804.75,E,4,12,0.9,41.0,M,23.1,M,1.0,0000*5A"), Some(4));
        assert_eq!(parse_gga_quality("$GPGGA,081530.00,6003.79,N,01804.75,E,5,08,1.1,41.0,M,,,,*00"), Some(5));
        assert_eq!(parse_gga_quality("$GNGGA,,,,,,0,00,,,,,,,*66"), Some(0));
        assert_eq!(parse_gga_quality("$GNRMC,081530.00,A,6003.79,N,01804.75,E,0.0,,280926,,,A*00"), None);
        assert_eq!(parse_gga_quality("skräp"), None);
    }

    #[test]
    fn styrkortsfragan_ar_ofarlig() {
        // 58 = CMD_AP_GET_ROUTE_PART (läser ruttlängd, ingen timeout_reset). 59 vore
        // CMD_AP_SET_ACTIVE (startar autopiloten) — får aldrig skickas härifrån.
        assert_eq!(BOARD_QUERY, [255, 58, 0, 0, 0, 0, 0]);
    }

    #[test]
    fn fix_tander_bade_float_och_fix() {
        let st = Status { fix_quality: Some(4), ..Default::default() };
        let app = App::new(Arc::new(Mutex::new(st)), true);
        let t = app.tiles();
        let float = t.iter().find(|t| t.title == "RTK Float").unwrap();
        let fix = t.iter().find(|t| t.title == "RTK Fix").unwrap();
        assert_eq!(float.color, BOX_OK);
        assert_eq!(fix.color, BOX_OK);
        let st = Status { fix_quality: Some(5), ..Default::default() };
        let t = App::new(Arc::new(Mutex::new(st)), true).tiles();
        assert_eq!(t.iter().find(|t| t.title == "RTK Float").unwrap().color, BOX_OK);
        assert_eq!(t.iter().find(|t| t.title == "RTK Fix").unwrap().color, BOX_IDLE);
    }

    #[test]
    fn tjanst_gar_bara_att_trycka_pa_nar_den_inte_ar_gron() {
        let app = App::new(Arc::new(Mutex::new(Status::default())), true);
        assert_eq!(app.service_tile("Car_Client", false, Action::RestartCarClient).action, Some(Action::RestartCarClient));
        assert_eq!(app.service_tile("Car_Client", true, Action::RestartCarClient).action, None);
        // Internet (WireGuard) likadant, Pi-rutan går alltid att trycka på.
        let t = app.tiles();
        assert_eq!(t.iter().find(|t| t.title == "Internet").unwrap().action, Some(Action::RestartWireGuard));
        let st = Status { handshake_age: Some(20), ..Default::default() };
        let t = App::new(Arc::new(Mutex::new(st)), true).tiles();
        assert_eq!(t.iter().find(|t| t.title == "Internet").unwrap().action, None);
        let t = app.tiles();
        assert_eq!(t.iter().find(|t| t.title == "Pi").unwrap().action, Some(Action::Reboot));
    }

    #[test]
    fn omstart_spärras_i_tio_sekunder() {
        let mut app = App::new(Arc::new(Mutex::new(Status::default())), true);
        app.run(Action::RestartCarClient);
        let tile = app.service_tile("Car_Client", false, Action::RestartCarClient);
        assert_eq!(tile.detail, "Startar om...");
        assert_eq!(tile.action, None);
        assert!(tile.pulsing);
    }
}

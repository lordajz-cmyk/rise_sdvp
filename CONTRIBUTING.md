# Så jobbar vi i rise_sdvp

Kortversion av arbetssättet för Mapro Systems AB och SLU. Koden körs på maskiner som Mapro säljer, därför
godkänner Mapro (`@maprosystemsab`) allt som går in i `master`.

## Grenar

- **`master`** är den gemensamma koden som vi vet fungerar. Robotarna och kundernas datorer hämtar uppdateringar
  härifrån, därför heter den `master` (inte `main`). Den är skyddad: ändringar går in via Pull Request.
- **En egen gren per person**, t.ex. `gunnar`. Jobba alltid där, aldrig direkt i `master`.
- **Experiment** som kanske aldrig blir av läggs i en egen gren, t.ex. `experiment/differential`, och raderas om
  de skrotas.

## Dagligen

```
git switch gunnar          # din gren
...ändra, bygg, prova...
git add -A
git commit -m "vad och varför"
git push

git switch master && git pull      # hämta det andra har gjort
git switch gunnar && git merge master
```

Hämta `master` till din gren ofta, gärna varje dag. Då blir konflikterna små. Pusha varje dag även om det inte
fungerar än – det är din gren och påverkar ingen annan.

## In i master

1. Öppna en **Pull Request** från din gren till `master` på GitHub och fyll i mallen (vad, vilka maskiner, hur
   provat).
2. Mapro granskar och godkänner. Fastnat eller osäker? Öppna en **Draft**-PR och fråga i kommentarerna.
3. Efter merge: `git switch master && git pull`, sedan `git merge master` i din gren.

Fel som redan kommit in i `master` rättas med `git revert <commit>` eller med en ny rättning. Aldrig force-push.

## Firmware (styrkortet)

- Maskinen väljs vid bygget: `make mactrac`, `make robant`, `make drangen`, `make rovmcu`, eller
  `flash_styrkort.sh --maskin <maskin>`. Använd `#ifdef IS_MACTRAC` / `IS_ROBANT` för maskinspecifik kod så att
  de andra maskinernas firmware inte ändras.
- Bygg **alla** maskiner innan Pull Request, inte bara den du jobbar med.
- Ny firmware provas först med maskinen **upphissad**, sedan på underlag.
- Det som körs ute är märkt med versionstaggar, t.ex. `fw-30.3-mactrac` och `fw-30.3-robant`. Dit kan man alltid
  gå tillbaka.

## Commit-meddelanden

Skriv vad och varför, som om du förklarar för kollegan: "Autopiloten sätter körriktningen: hastighetsgivaren
räknade framåt som bakåt", inte "fix". Gärna med typ först, t.ex. `fix(autopilot): …`, `feat(car_client): …`,
`docs: …`.

## Aldrig i repot

Lösenord, Swepos/NTRIP-uppgifter, WireGuard-nycklar, `data.db` eller kunduppgifter. Repot är publikt.

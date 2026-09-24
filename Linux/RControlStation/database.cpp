#include "database.h"
#include <QtWidgets>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

database::database(QWidget* _qw) {
    qw=_qw;
    // Initialize the database:
    QSqlError err = initDb();
    if (err.type() != QSqlError::NoError) {
        showError(err);
        return;
    }
    
    // Ensure required tables exist
    ensureControllersTableExists();
    ensureControlsTableExists();
    ensureActuatorsTableExists();
    ensureSensorsTableExists();
    ensureControlRelationshipsExist();
}

QVariant database::addFarm(const QString &name)
{
    const auto INSERT_LOCATION_SQL = QLatin1String(R"(
        insert into locations(name)
                          values(?)
        )");
    QSqlQuery q;
    if (q.prepare(INSERT_LOCATION_SQL))
    {
        q.addBindValue(name);
        q.exec();
        return q.lastInsertId();
    };

}

void database::addPath(const QString &pathname, const QString &xmlstring,const QVariant &fieldId)
{
    const auto INSERT_FIELD_SQL = QLatin1String(R"(
            insert into paths(name,field,xml)
                              values(?,?,?)
            )");
    QSqlQuery q;
    if (q.prepare(INSERT_FIELD_SQL))
    {
        q.addBindValue(pathname);
        q.addBindValue(fieldId);
        q.addBindValue(xmlstring);
        q.exec();
    };
}

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>

void database::updateFarmLocation(int farmId, double latitude, double longitude)
{
    QString queryString =
        "UPDATE locations "
        "SET longitude=:longitude,latitude=:latitude "
        "WHERE id=:locationid";

    QSqlQuery query;
    query.prepare(queryString);

    query.bindValue(":longitude", longitude);
    query.bindValue(":latitude", latitude);
    query.bindValue(":locationid", farmId);

    qDebug() << "Prepared Query:" << queryString;
    qDebug() << "Bound Values:" << longitude << latitude << farmId;

    if (!query.exec()) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
    } else {
        qDebug() << "Query executed successfully.";
    }
}

/*
void database::updateFarmLocation(int farmid,double latitude, double longitude)
{
    QSqlQuery q;
    if (q.prepare("UPDATE locations SET longitude = :longitude, latitude = :latitude WHERE locationid = :farmid"))
    {
        q.addBindValue(":latitude",QVariant(longitude));
        q.addBindValue(":longitude",QVariant(latitude));
        q.addBindValue(":farmid",farmid);
        if (!q.exec())
        {
            qDebug() << "Failed to execute query:" << q.lastError();
        }
    } else
    {
        qDebug() << "Failed to prepare query:" << q.lastError();
    }
};
*/

void database::addField(QString fieldname,int farmid,QString filename)
{
    const auto INSERT_FIELD_SQL = QLatin1String(R"(
        insert into fields(name,location,storedinfile)
                          values(?,?,?)
        )");
    QSqlQuery q;
    if (q.prepare(INSERT_FIELD_SQL))
    {
        q.addBindValue(fieldname);
        q.addBindValue(farmid);
        q.addBindValue(filename);
        q.exec();
    };
}

void database::deleteField(const QVariant &fieldId)
{
    const auto DELETE_FIELD_SQL = QLatin1String(R"(DELETE FROM fields WHERE id=:fieldid)");
    QSqlQuery q;
    if (q.prepare(DELETE_FIELD_SQL))
    {
        q.bindValue(":fieldid",fieldId);
        q.exec();
        qDebug() << "DELETE FIELD!!!: " << fieldId;
    } else {
        qDebug() << "Oups..";
    };
}

void database::showError(const QSqlError &err)
{
    QMessageBox::critical(qw, "Unable to initialize Database",
                          "Error initializing database: " + err.text());
}


QSqlError database::initDb()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    
    // Try different locations for the database file. Ordered so that the path tied to
    // the BINARY's own location (deterministic, independent of the terminal's current
    // directory) is tried before the working-directory-relative one — otherwise a stray
    // "data.db" left behind in whatever folder the app happens to be launched from
    // (e.g. $HOME) silently wins over the real project database every time.
    QStringList dbPaths;

    // 1. AppImage data directory or local application directory (next to the binary)
    QString appImagePath = QCoreApplication::applicationDirPath();
    dbPaths << appImagePath + "/data.db";
    dbPaths << appImagePath + "/../share/RControlStation/data.db";
    dbPaths << appImagePath + "/../../share/RControlStation/data.db";
    dbPaths << "/usr/share/RControlStation/data.db";

    // 2. Common data directories
    dbPaths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data.db";
    // DataLocation doesn't exist in Qt 6, use AppDataLocation instead
    // dbPaths << QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/data.db";

    // 3. Current directory last (for development only — e.g. running via `make run`
    // straight from the source tree without a proper install).
    dbPaths << "data.db";
    
    // Try each path until we find a working, EXISTING database. SQLite's db.open()
    // silently creates a fresh empty file if none exists at the given path, so without
    // this QFile::exists() check the very first candidate ("data.db", relative to the
    // current working directory) always "succeeds" — creating/opening a throwaway
    // database whenever the app is launched from a different directory, while the
    // real project database further down the list is never even tried.
    foreach (const QString &path, dbPaths) {
        if (!QFile::exists(path)) {
            continue;
        }
        db.setDatabaseName(path);
        if (db.open()) {
            qDebug() << "Database opened from:" << path;
            return QSqlError();
        }
    }
    
    // If no database found, create one in the writable data location
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(defaultPath);
    db.setDatabaseName(defaultPath + "/data.db");
    if (!db.open())
        return db.lastError();
    
    qDebug() << "Created new database at:" << defaultPath + "/data.db";
    return QSqlError();
}

QSqlDatabase database::getDb()
{
    return db;
}

// Old actions table methods - kept for compatibility but not used
void database::ensureActionsTableExists() {
    // Actions table is now replaced by controls, actuators, and sensors
    // This method is kept for compatibility but does nothing
}

void database::insertDefaultActions() {
    // Replaced by insertDefaultControls(), insertDefaultActuators(), insertDefaultSensors()
}

void database::updateActionsWithGeneratedColours() {
    // No longer needed as new tables have colour columns by default
}



QString database::generateColourForAction(int actionIndex)
{
    // Generate a distinct colour using HSV colour space for better visual distinction
    // Use golden ratio conjugation for good distribution
    int hue = (actionIndex * 137) % 360;  // Golden ratio (≈137.5°) for good distribution
    
    // Convert HSV to RGB and then to hex string
    QColor colour;
    colour.setHsv(hue, 200, 255);  // Saturate and brighten for vibrant colours
    
    return colour.name(QColor::HexRgb);  // Returns "#RRGGBB" format
}

void database::ensureControllersTableExists()
{
    QSqlQuery query(db);
    
    // Check if controllers table exists
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='controllers'")) {
        qDebug() << "Error checking for controllers table:" << query.lastError().text();
        return;
    }
    
    // If table doesn't exist, create it
    if (!query.next()) {
        QString createTableSql = 
            "CREATE TABLE controllers (" 
            "    id INTEGER PRIMARY KEY AUTOINCREMENT," 
            "    name TEXT," 
            "    action INTEGER" 
            ");";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating controllers table:" << query.lastError().text();
            return;
        }
        
        qDebug() << "Controllers table created successfully.";

        // Standardbindningar för en ny databas, samma som på RobAnt: vänster spak
        // upp/ner (5) = Speed Control (10), höger spak sidled (8) = Steering Control (11).
        // Aktivitetsnumren måste stämma med styrkortets aktuatorer (och robotd).
        QSqlQuery ins(db);
        ins.exec("INSERT INTO controllers (id, name, action) VALUES (5, NULL, 10)");
        ins.exec("INSERT INTO controllers (id, name, action) VALUES (8, NULL, 11)");
    }
}


// New control system methods
void database::ensureControlsTableExists()
{
    QSqlQuery query(db);
    
    // Check if controls table exists
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='controls'")) {
        qDebug() << "Error checking for controls table:" << query.lastError().text();
        return;
    }
    
    // If table doesn't exist, create it
    if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE controls ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "type TEXT NOT NULL DEFAULT 'logical', "  // 'logical' or 'pid'
            "target_value REAL DEFAULT 0.0, "
            "is_active BOOLEAN DEFAULT 0, "
            "colour TEXT DEFAULT '#CCCCCC', "
            "pid_kp REAL DEFAULT 1.0, "
            "pid_ki REAL DEFAULT 0.0, "
            "pid_kd REAL DEFAULT 0.0, "
            "pid_output_min REAL DEFAULT 0.0, "
            "pid_output_max REAL DEFAULT 100.0, "
            "logical_operation TEXT DEFAULT 'AND'"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating controls table:" << query.lastError().text();
            return;
        }
        
        qDebug() << "Created controls table";
        
        // Add some default controls if the table is new
        insertDefaultControls();
    } else {
        // Table exists, check if all columns exist
        QSqlQuery columnQuery(db);
        if (columnQuery.exec("PRAGMA table_info(controls)")) {
            QSet<QString> existingColumns;
            while (columnQuery.next()) {
                existingColumns.insert(columnQuery.value(1).toString());
            }
            
            // Add missing columns if needed
            if (!existingColumns.contains("type")) {
                query.exec("ALTER TABLE controls ADD COLUMN type TEXT NOT NULL DEFAULT 'logical'");
            }
            if (!existingColumns.contains("target_value")) {
                query.exec("ALTER TABLE controls ADD COLUMN target_value REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("is_active")) {
                query.exec("ALTER TABLE controls ADD COLUMN is_active BOOLEAN DEFAULT 0");
            }
            if (!existingColumns.contains("colour")) {
                query.exec("ALTER TABLE controls ADD COLUMN colour TEXT DEFAULT '#CCCCCC'");
            }
            if (!existingColumns.contains("pid_kp")) {
                query.exec("ALTER TABLE controls ADD COLUMN pid_kp REAL DEFAULT 1.0");
            }
            if (!existingColumns.contains("pid_ki")) {
                query.exec("ALTER TABLE controls ADD COLUMN pid_ki REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("pid_kd")) {
                query.exec("ALTER TABLE controls ADD COLUMN pid_kd REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("pid_output_min")) {
                query.exec("ALTER TABLE controls ADD COLUMN pid_output_min REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("pid_output_max")) {
                query.exec("ALTER TABLE controls ADD COLUMN pid_output_max REAL DEFAULT 100.0");
            }
            if (!existingColumns.contains("logical_operation")) {
                query.exec("ALTER TABLE controls ADD COLUMN logical_operation TEXT DEFAULT 'AND'");
            }
        }
        
        // Self-healing database check: If controls table is empty, contains only 1 row, OR is missing "Steering Control",
        // reset the table and insert the full set of default controls!
        bool needRestore = false;
        QSqlQuery checkQuery("SELECT COUNT(*) FROM controls", db);
        if (checkQuery.exec() && checkQuery.next()) {
            int count = checkQuery.value(0).toInt();
            if (count <= 1) {
                needRestore = true;
            }
        }
        
        if (!needRestore) {
            QSqlQuery checkSteering("SELECT COUNT(*) FROM controls WHERE name = 'Steering Control'", db);
            if (checkSteering.exec() && checkSteering.next()) {
                if (checkSteering.value(0).toInt() == 0) {
                    needRestore = true;
                }
            }
        }
        
        if (needRestore) {
            qDebug() << "Controls table needs restoration. Restoring full default control system...";
            query.exec("DELETE FROM controls");
            insertDefaultControls();
        }
    }
}

void database::ensureActuatorsTableExists()
{
    QSqlQuery query(db);
    
    // Check if actuators table exists
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='actuators'")) {
        qDebug() << "Error checking for actuators table:" << query.lastError().text();
        return;
    }
    
    // If table doesn't exist, create it
    if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE actuators ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "type TEXT NOT NULL, "
            "current_position REAL DEFAULT 0.0, "
            "min_position REAL DEFAULT 0.0, "
            "max_position REAL DEFAULT 100.0, "
            "units TEXT DEFAULT '%', "
            "colour TEXT DEFAULT '#CCCCCC', "
            "is_reversible BOOLEAN DEFAULT 1"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating actuators table:" << query.lastError().text();
            return;
        }
        
        qDebug() << "Created actuators table";
        
        // Add some default actuators if the table is new
        insertDefaultActuators();
    } else {
        // Table exists, check if all columns exist
        QSqlQuery columnQuery(db);
        if (columnQuery.exec("PRAGMA table_info(actuators)")) {
            QSet<QString> existingColumns;
            while (columnQuery.next()) {
                existingColumns.insert(columnQuery.value(1).toString());
            }
            
            // Add missing columns if needed
            if (!existingColumns.contains("type")) {
                query.exec("ALTER TABLE actuators ADD COLUMN type TEXT NOT NULL DEFAULT 'Motor'");
            }
            if (!existingColumns.contains("current_position")) {
                query.exec("ALTER TABLE actuators ADD COLUMN current_position REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("min_position")) {
                query.exec("ALTER TABLE actuators ADD COLUMN min_position REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("max_position")) {
                query.exec("ALTER TABLE actuators ADD COLUMN max_position REAL DEFAULT 100.0");
            }
            if (!existingColumns.contains("units")) {
                query.exec("ALTER TABLE actuators ADD COLUMN units TEXT DEFAULT '%'");
            }
            if (!existingColumns.contains("colour")) {
                query.exec("ALTER TABLE actuators ADD COLUMN colour TEXT DEFAULT '#CCCCCC'");
            }
            if (!existingColumns.contains("is_reversible")) {
                query.exec("ALTER TABLE actuators ADD COLUMN is_reversible BOOLEAN DEFAULT 1");
            }
        }
    }
}

void database::ensureSensorsTableExists()
{
    QSqlQuery query(db);
    
    // Check if sensors table exists
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='sensors'")) {
        qDebug() << "Error checking for sensors table:" << query.lastError().text();
        return;
    }
    
    // If table doesn't exist, create it
    if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE sensors ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "type TEXT NOT NULL, "
            "current_value REAL DEFAULT 0.0, "
            "min_value REAL DEFAULT 0.0, "
            "max_value REAL DEFAULT 100.0, "
            "units TEXT DEFAULT '%', "
            "colour TEXT DEFAULT '#CCCCCC', "
            "calibration_offset REAL DEFAULT 0.0, "
            "calibration_scale REAL DEFAULT 1.0"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating sensors table:" << query.lastError().text();
            return;
        }
        
        qDebug() << "Created sensors table";
        
        // Add some default sensors if the table is new
        insertDefaultSensors();
    } else {
        // Table exists, check if all columns exist
        QSqlQuery columnQuery(db);
        if (columnQuery.exec("PRAGMA table_info(sensors)")) {
            QSet<QString> existingColumns;
            while (columnQuery.next()) {
                existingColumns.insert(columnQuery.value(1).toString());
            }
            
            // Add missing columns if needed
            if (!existingColumns.contains("type")) {
                query.exec("ALTER TABLE sensors ADD COLUMN type TEXT NOT NULL DEFAULT 'Position'");
            }
            if (!existingColumns.contains("current_value")) {
                query.exec("ALTER TABLE sensors ADD COLUMN current_value REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("min_value")) {
                query.exec("ALTER TABLE sensors ADD COLUMN min_value REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("max_value")) {
                query.exec("ALTER TABLE sensors ADD COLUMN max_value REAL DEFAULT 100.0");
            }
            if (!existingColumns.contains("units")) {
                query.exec("ALTER TABLE sensors ADD COLUMN units TEXT DEFAULT '%'");
            }
            if (!existingColumns.contains("colour")) {
                query.exec("ALTER TABLE sensors ADD COLUMN colour TEXT DEFAULT '#CCCCCC'");
            }
            if (!existingColumns.contains("calibration_offset")) {
                query.exec("ALTER TABLE sensors ADD COLUMN calibration_offset REAL DEFAULT 0.0");
            }
            if (!existingColumns.contains("calibration_scale")) {
                query.exec("ALTER TABLE sensors ADD COLUMN calibration_scale REAL DEFAULT 1.0");
            }
        }
    }
}

void database::ensureControlRelationshipsExist()
{
    QSqlQuery query(db);
    
    // Create control_actuators table
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='control_actuators'")) {
        qDebug() << "Error checking for control_actuators table:" << query.lastError().text();
    } else if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE control_actuators ("
            "control_id INTEGER NOT NULL, "
            "actuator_id INTEGER NOT NULL, "
            "target_position REAL DEFAULT 0.0, "
            "PRIMARY KEY (control_id, actuator_id), "
            "FOREIGN KEY (control_id) REFERENCES controls(id), "
            "FOREIGN KEY (actuator_id) REFERENCES actuators(id)"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating control_actuators table:" << query.lastError().text();
        } else {
            qDebug() << "Created control_actuators table";
        }
    }
    
    // Create control_sensors table
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='control_sensors'")) {
        qDebug() << "Error checking for control_sensors table:" << query.lastError().text();
    } else if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE control_sensors ("
            "control_id INTEGER NOT NULL, "
            "sensor_id INTEGER NOT NULL, "
            "target_value REAL, "
            "hysteresis REAL DEFAULT 1.0, "
            "is_primary BOOLEAN DEFAULT 0, "
            "PRIMARY KEY (control_id, sensor_id), "
            "FOREIGN KEY (control_id) REFERENCES controls(id), "
            "FOREIGN KEY (sensor_id) REFERENCES sensors(id)"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating control_sensors table:" << query.lastError().text();
        } else {
            qDebug() << "Created control_sensors table";
        }
    }
    
    // Create sensor_attribute_mappings table
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='sensor_attribute_mappings'")) {
        qDebug() << "Error checking for sensor_attribute_mappings table:" << query.lastError().text();
    } else if (!query.next()) {
        QString createTableSql =
            "CREATE TABLE sensor_attribute_mappings ("
            "sensor_id INTEGER NOT NULL, "
            "attribute_value INTEGER NOT NULL, "
            "condition TEXT NOT NULL, "
            "threshold_min REAL, "
            "threshold_max REAL, "
            "PRIMARY KEY (sensor_id, attribute_value), "
            "FOREIGN KEY (sensor_id) REFERENCES sensors(id)"
            ")";
        
        if (!query.exec(createTableSql)) {
            qDebug() << "Error creating sensor_attribute_mappings table:" << query.lastError().text();
        } else {
            qDebug() << "Created sensor_attribute_mappings table";
        }
    }
}

void database::insertDefaultControls()
{
    // Insert some default controls if the table is empty
    QList<QPair<QString, QString>> defaultControls = {
        {"Front Lift Control", "logical"},
        {"Rear Lift Control", "logical"},
        {"Implement Position", "pid"},
        {"Speed Control", "pid"},
        {"Steering Control", "pid"},
        {"Emergency Stop", "logical"}
    };
    
    QSqlQuery query(db);
    // Fasta id 7–12, samma som i befintliga databaser: id:t skickas som aktivitet
    // till styrkortet (CMD_RC_CONTROL_ADV), och styrkortets aktuatorer och robotd
    // använder Speed Control = 10 och Steering Control = 11. Med AUTOINCREMENT
    // blev de 4 och 5 i en ny databas, och då rörde sig ingenting.
    const int firstId = 7;
    query.prepare("INSERT INTO controls (id, name, type, target_value, is_active, colour, pid_kp, pid_ki, pid_kd, pid_output_min, pid_output_max, logical_operation) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    
    for (int i = 0; i < defaultControls.size(); i++) {
        query.addBindValue(firstId + i);
        query.addBindValue(defaultControls[i].first);
        query.addBindValue(defaultControls[i].second);
        
        // Set appropriate values based on control type
        if (defaultControls[i].second == "pid") {
            query.addBindValue(50.0); // target_value
            query.addBindValue(0);   // is_active
            query.addBindValue(generateColourForAction(i));
            query.addBindValue(1.0); // pid_kp
            query.addBindValue(0.1); // pid_ki
            query.addBindValue(0.01); // pid_kd
            query.addBindValue(0.0); // pid_output_min
            query.addBindValue(100.0); // pid_output_max
            query.addBindValue("AND"); // logical_operation (not used for PID)
        } else {
            query.addBindValue(100.0); // target_value
            query.addBindValue(0);    // is_active
            query.addBindValue(generateColourForAction(i));
            query.addBindValue(1.0); // pid_kp (not used for logical)
            query.addBindValue(0.0); // pid_ki (not used for logical)
            query.addBindValue(0.0); // pid_kd (not used for logical)
            query.addBindValue(0.0); // pid_output_min (not used for logical)
            query.addBindValue(100.0); // pid_output_max (not used for logical)
            query.addBindValue("AND"); // logical_operation
        }
        
        if (!query.exec()) {
            qDebug() << "Error inserting default control:" << query.lastError().text();
        }
    }
}

void database::insertDefaultActuators()
{
    // Insert some default actuators if the table is empty
    QStringList defaultActuators = {
        "Front Lift Motor", "Rear Lift Motor", "Implement Motor", 
        "Left Drive Motor", "Right Drive Motor", "PTO Motor"
    };
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO actuators (name, type, current_position, min_position, max_position, units, colour, is_reversible) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    
    for (int i = 0; i < defaultActuators.size(); i++) {
        query.addBindValue(defaultActuators[i]);
        query.addBindValue("Motor");
        query.addBindValue(0.0); // current_position
        query.addBindValue(0.0); // min_position
        query.addBindValue(100.0); // max_position
        query.addBindValue("%"); // units
        query.addBindValue(generateColourForAction(i)); // colour
        query.addBindValue(i < 4 ? 1 : 0); // is_reversible (PTO might not be reversible)
        
        if (!query.exec()) {
            qDebug() << "Error inserting default actuator:" << query.lastError().text();
        }
    }
}

void database::insertDefaultSensors()
{
    // Insert some default sensors if the table is empty
    QList<QPair<QString, QString>> defaultSensors = {
        {"Front Lift Position", "Position"},
        {"Rear Lift Position", "Position"},
        {"Implement Position", "Position"},
        {"Hydraulic Pressure", "Pressure"},
        {"Battery Voltage", "Voltage"}
    };
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO sensors (name, type, current_value, min_value, max_value, units, colour, calibration_offset, calibration_scale) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    
    for (int i = 0; i < defaultSensors.size(); i++) {
        query.addBindValue(defaultSensors[i].first);
        query.addBindValue(defaultSensors[i].second);
        query.addBindValue(0.0); // current_value
        query.addBindValue(0.0); // min_value
        
        // Set max value based on sensor type
        if (defaultSensors[i].second == "Pressure") {
            query.addBindValue(250.0); // bar
            query.addBindValue("bar"); // units
        } else if (defaultSensors[i].second == "Voltage") {
            query.addBindValue(30.0); // volts
            query.addBindValue("V"); // units
        } else {
            query.addBindValue(100.0); // percent
            query.addBindValue("%"); // units
        }
        
        query.addBindValue(generateColourForAction(i)); // colour
        query.addBindValue(0.0); // calibration_offset
        query.addBindValue(1.0); // calibration_scale
        
        if (!query.exec()) {
            qDebug() << "Error inserting default sensor:" << query.lastError().text();
        }
    }
}

// Controller methods
QList<ControllerInfo> database::getAllControllers()
{
    QList<ControllerInfo> controllers;
    
    QSqlQuery query("SELECT id, name FROM controls ORDER BY name", db);
    if (query.exec()) {
        while (query.next()) {
            ControllerInfo info;
            info.id = query.value(0).toInt();
            info.name = query.value(1).toString();
            controllers.append(info);
        }
    } else {
        qDebug() << "Error getting controls:" << query.lastError().text();
    }
    
    return controllers;
}

ControllerInfo database::getControllerById(int id)
{
    ControllerInfo info;
    info.id = -1;
    info.name = "";
    
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM controls WHERE id = ?");
    query.addBindValue(id);
    
    if (query.exec() && query.next()) {
        info.id = query.value(0).toInt();
        info.name = query.value(1).toString();
    } else {
        qDebug() << "Error getting control by ID:" << query.lastError().text();
    }
    
    return info;
}

void database::addController(const QString& name)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO controls (name) VALUES (?)");
    query.addBindValue(name);
    
    if (!query.exec()) {
        qDebug() << "Error adding control:" << query.lastError().text();
    }
}

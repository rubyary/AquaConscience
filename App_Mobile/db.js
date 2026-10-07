const Database = require('better-sqlite3');
const path = require('path');

const db = new Database(path.join(__dirname, 'aquaconscience.db'));
db.pragma('journal_mode = WAL');
db.pragma('foreign_keys = ON');

db.exec(`
CREATE TABLE IF NOT EXISTS users (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  name          TEXT NOT NULL,
  email         TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  created_at    INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS households (
  user_id   INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
  household TEXT NOT NULL CHECK (household IN ('solo','familia')),
  members   INTEGER NOT NULL DEFAULT 1,
  type      TEXT NOT NULL CHECK (type IN ('general','puntos'))
);

CREATE TABLE IF NOT EXISTS sensors (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name       TEXT NOT NULL,
  loc        TEXT NOT NULL,
  api_key    TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL
);

-- Consumo detectado por cada dispositivo
CREATE TABLE IF NOT EXISTS readings (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  sensor_id  INTEGER NOT NULL REFERENCES sensors(id) ON DELETE CASCADE,
  user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  liters     REAL NOT NULL CHECK (liters >= 0),
  activity   TEXT,
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_readings_user_time ON readings(user_id, created_at);
CREATE INDEX IF NOT EXISTS idx_sensors_user ON sensors(user_id);
`);

module.exports = db;

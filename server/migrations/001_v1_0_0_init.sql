-- 001_v1_0_0_init.sql
-- Schema version: 1, app_version: v1.0.0

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS tasks (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT    NOT NULL,
    type         INTEGER NOT NULL,
    params       TEXT    NOT NULL,
    status       TEXT    NOT NULL,
    exit_code    INTEGER NOT NULL DEFAULT 0,
    last_output  TEXT    NOT NULL DEFAULT '',
    last_error   TEXT    NOT NULL DEFAULT '',
    create_time  TEXT    NOT NULL,
    update_time  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    username    TEXT NOT NULL UNIQUE,
    password    TEXT NOT NULL,
    create_time TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS taskhub_meta (
    id             INTEGER PRIMARY KEY,
    schema_version INTEGER NOT NULL,
    app_version    TEXT    NOT NULL,
    upgrade_time   TEXT    NOT NULL
);

INSERT OR REPLACE INTO taskhub_meta (id, schema_version, app_version, upgrade_time)
VALUES (1, 1, 'v1.0.0', datetime('now'));

COMMIT;
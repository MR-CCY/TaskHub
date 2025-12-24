BEGIN TRANSACTION;

ALTER TABLE dag_run ADD COLUMN name TEXT NOT NULL DEFAULT '';

CREATE TABLE IF NOT EXISTS task_event (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id        TEXT NOT NULL DEFAULT '',
  task_id       TEXT NOT NULL DEFAULT '',
  type          TEXT NOT NULL,
  event         TEXT NOT NULL,
  ts_ms         INTEGER NOT NULL,
  payload_json  TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_task_event_run    ON task_event(run_id);
CREATE INDEX IF NOT EXISTS idx_task_event_task   ON task_event(task_id);
CREATE INDEX IF NOT EXISTS idx_task_event_type   ON task_event(type);
CREATE INDEX IF NOT EXISTS idx_task_event_event  ON task_event(event);
CREATE INDEX IF NOT EXISTS idx_task_event_ts     ON task_event(ts_ms DESC);

UPDATE taskhub_meta
SET schema_version = 5,
    app_version    = 'v1.0.4',
    upgrade_time   = datetime('now')
WHERE id = 1;

COMMIT;

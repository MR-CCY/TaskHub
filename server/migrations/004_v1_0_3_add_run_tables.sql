BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS dag_run (
  run_id             TEXT PRIMARY KEY,
  source             TEXT NOT NULL DEFAULT 'manual',
  template_id        TEXT,
  cron_job_id        TEXT,
  fail_policy        TEXT NOT NULL DEFAULT 'SkipDownstream',
  max_parallel       INTEGER NOT NULL DEFAULT 1,
  status             INTEGER NOT NULL DEFAULT 0,  -- 0=Running 1=Success 2=Failed 3=Canceled 4=Unknown
  message            TEXT NOT NULL DEFAULT '',
  start_ts_ms        INTEGER NOT NULL,
  end_ts_ms          INTEGER NOT NULL DEFAULT 0,
  dag_json           TEXT NOT NULL,
  workflow_json      TEXT NOT NULL DEFAULT '',
  total              INTEGER NOT NULL DEFAULT 0,
  success_count      INTEGER NOT NULL DEFAULT 0,
  failed_count       INTEGER NOT NULL DEFAULT 0,
  skipped_count      INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_dag_run_start_ts ON dag_run(start_ts_ms DESC);
CREATE INDEX IF NOT EXISTS idx_dag_run_status   ON dag_run(status);
CREATE INDEX IF NOT EXISTS idx_dag_run_source   ON dag_run(source);

CREATE TABLE IF NOT EXISTS task_run (
  id                 INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id             TEXT NOT NULL,
  logical_id         TEXT NOT NULL,
  task_id            TEXT NOT NULL DEFAULT '',
  name               TEXT NOT NULL DEFAULT '',
  exec_type          TEXT NOT NULL,
  exec_command       TEXT NOT NULL DEFAULT '',
  exec_params_json   TEXT NOT NULL DEFAULT '{}',
  deps_json          TEXT NOT NULL DEFAULT '[]',
  status             INTEGER NOT NULL DEFAULT 0, -- Pending/Running/Success/Failed/Skipped/Canceled/Timeout...
  exit_code          INTEGER NOT NULL DEFAULT 0,
  duration_ms        INTEGER NOT NULL DEFAULT 0,
  message            TEXT NOT NULL DEFAULT '',
  stdout             TEXT NOT NULL DEFAULT '',
  stderr             TEXT NOT NULL DEFAULT '',
  attempt            INTEGER NOT NULL DEFAULT 1,
  max_attempts       INTEGER NOT NULL DEFAULT 1,
  worker_id          TEXT NOT NULL DEFAULT '',
  worker_host        TEXT NOT NULL DEFAULT '',
  worker_port        INTEGER NOT NULL DEFAULT 0,
  start_ts_ms        INTEGER NOT NULL DEFAULT 0,
  end_ts_ms          INTEGER NOT NULL DEFAULT 0,
  metadata_json      TEXT NOT NULL DEFAULT '{}',
  FOREIGN KEY(run_id) REFERENCES dag_run(run_id) ON DELETE CASCADE
);
CREATE UNIQUE INDEX IF NOT EXISTS uq_task_run_run_logical ON task_run(run_id, logical_id);
CREATE INDEX IF NOT EXISTS idx_task_run_runid   ON task_run(run_id);
CREATE INDEX IF NOT EXISTS idx_task_run_taskid  ON task_run(task_id);
CREATE INDEX IF NOT EXISTS idx_task_run_status  ON task_run(status);

CREATE TABLE IF NOT EXISTS task_template (
  template_id        TEXT PRIMARY KEY,
  name               TEXT NOT NULL,
  description        TEXT NOT NULL DEFAULT '',
  exec_type          TEXT NOT NULL,
  exec_command       TEXT NOT NULL DEFAULT '',
  exec_params_json   TEXT NOT NULL DEFAULT '{}',
  timeout_ms         INTEGER NOT NULL DEFAULT 0,
  retry_count        INTEGER NOT NULL DEFAULT 0,
  retry_delay_ms     INTEGER NOT NULL DEFAULT 1000,
  retry_exp_backoff  INTEGER NOT NULL DEFAULT 1,
  priority           INTEGER NOT NULL DEFAULT 0,
  queue              TEXT NOT NULL DEFAULT 'default',
  capture_output     INTEGER NOT NULL DEFAULT 1,
  metadata_json      TEXT NOT NULL DEFAULT '{}',
  created_ts_ms      INTEGER NOT NULL,
  updated_ts_ms      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_task_tpl_type ON task_template(exec_type);

UPDATE taskhub_meta
SET schema_version = 4,
    app_version    = 'v1.0.3',
    upgrade_time   = datetime('now')
WHERE id = 1;

COMMIT;

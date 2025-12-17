BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS task_templates (
  template_id        TEXT PRIMARY KEY,
  name               TEXT DEFAULT '',
  description        TEXT DEFAULT '',
  task_json_template TEXT NOT NULL,  
  schema_json        TEXT NOT NULL, 
  created_at_ms      INTEGER NOT NULL,
  updated_at_ms      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_task_templates_updated_at ON task_templates(updated_at_ms);
UPDATE taskhub_meta
SET schema_version = 3,
    app_version    = 'v1.0.2',
    upgrade_time   = datetime('now')
WHERE id = 1;
COMMIT;
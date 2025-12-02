-- 002_v1_0_1_add_task_fields.sql
-- Schema version: 2, app_version: v1.0.1

BEGIN TRANSACTION;

ALTER TABLE tasks ADD COLUMN timeout_sec  INTEGER NOT NULL DEFAULT 0;
ALTER TABLE tasks ADD COLUMN max_retries  INTEGER NOT NULL DEFAULT 0;
ALTER TABLE tasks ADD COLUMN retry_count  INTEGER NOT NULL DEFAULT 0;
ALTER TABLE tasks ADD COLUMN cancel_flag  INTEGER NOT NULL DEFAULT 0;

UPDATE taskhub_meta
SET schema_version = 2,
    app_version    = 'v1.0.1',
    upgrade_time   = datetime('now')
WHERE id = 1;

COMMIT;
BEGIN TRANSACTION;

-- Add schema_json field to task_template table for parameter schema definition
ALTER TABLE task_template ADD COLUMN schema_json TEXT NOT NULL DEFAULT '{"params":[]}';

-- Seed built-in task templates (idempotent)
INSERT OR IGNORE INTO task_template (
  template_id, name, description, exec_type, exec_command, exec_params_json,
  schema_json, timeout_ms, retry_count, retry_delay_ms, retry_exp_backoff,
  priority, queue, capture_output, metadata_json, created_ts_ms, updated_ts_ms
) VALUES
  (
    'local_math_add', 'Math: Add', 'Add two numbers and return the result',
    'Local', 'math_add', '{"a":"0","b":"0"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"0"},{"name":"b","type":"int","required":true,"defaultValue":"0"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'local_math_sub', 'Math: Subtract', 'Subtract b from a and return the result',
    'Local', 'math_sub', '{"a":"0","b":"0"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"0"},{"name":"b","type":"int","required":true,"defaultValue":"0"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'local_math_mul', 'Math: Multiply', 'Multiply two numbers and return the result',
    'Local', 'math_mul', '{"a":"1","b":"1"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"1"},{"name":"b","type":"int","required":true,"defaultValue":"1"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'local_math_div', 'Math: Divide', 'Divide a by b and return the result',
    'Local', 'math_div', '{"a":"0","b":"1"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"0"},{"name":"b","type":"int","required":true,"defaultValue":"1"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'local_math_mod', 'Math: Modulo', 'Calculate a modulo b and return the result',
    'Local', 'math_mod', '{"a":"0","b":"1"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"0"},{"name":"b","type":"int","required":true,"defaultValue":"1"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'local_math_cmp', 'Math: Compare', 'Compare two numbers (returns ''gt'', ''lt'', or ''eq'')',
    'Local', 'math_cmp', '{"a":"0","b":"0"}',
    '{"params":[{"name":"a","type":"int","required":true,"defaultValue":"0"},{"name":"b","type":"int","required":true,"defaultValue":"0"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'shell_echo', 'Shell: Echo', 'Echo a message to stdout',
    'Shell', 'echo "{{message}}"', '{"message":"Hello, World!"}',
    '{"params":[{"name":"message","type":"string","required":true,"defaultValue":"Hello, World!"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'shell_sleep', 'Shell: Sleep', 'Sleep for a specified number of seconds',
    'Shell', 'sleep {{seconds}}', '{"seconds":"1"}',
    '{"params":[{"name":"seconds","type":"int","required":true,"defaultValue":"1"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'shell_curl', 'Shell: cURL', 'Make an HTTP request using cURL',
    'Shell', 'curl -X {{method}} "{{url}}"', '{"url":"http://example.com","method":"GET"}',
    '{"params":[{"name":"url","type":"string","required":true,"defaultValue":"http://example.com"},{"name":"method","type":"string","required":false,"defaultValue":"GET"}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'http_get', 'HTTP: GET Request', 'Make an HTTP GET request',
    'Http', '{{url}}', '{"url":"http://example.com","method":"GET","headers":"{}"}',
    '{"params":[{"name":"url","type":"string","required":true,"defaultValue":"http://example.com"},{"name":"headers","type":"json","required":false,"defaultValue":{}}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  ),
  (
    'http_post', 'HTTP: POST Request', 'Make an HTTP POST request',
    'Http', '{{url}}', '{"url":"http://example.com","method":"POST","headers":"{}","body":""}',
    '{"params":[{"name":"url","type":"string","required":true,"defaultValue":"http://example.com"},{"name":"headers","type":"json","required":false,"defaultValue":{}},{"name":"body","type":"string","required":false,"defaultValue":""}]}',
    0, 0, 1000, 1, 0, 'default', 1, '{}',
    CAST(strftime('%s','now') AS INTEGER) * 1000,
    CAST(strftime('%s','now') AS INTEGER) * 1000
  );

-- Update metadata
UPDATE taskhub_meta
SET schema_version = 6,
    app_version    = 'v1.0.5',
    upgrade_time   = datetime('now')
WHERE id = 1;

COMMIT;

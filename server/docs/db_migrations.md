
TaskHub 数据库迁移（DB Migration）设计说明

📌 概述

TaskHub 使用 内置的数据库版本管理机制（Database Migration），用于：
	•	自动创建全新的数据库（冷启动）
	•	自动升级旧数据库（平滑升级）
	•	保证业务逻辑与数据库结构一致
	•	在多版本迭代下保持可维护性和稳定性

数据库类型：SQLite3

迁移方式：版本号驱动 + SQL 脚本 + C++ 迁移引擎

⸻

🏷️ 当前版本信息

名称	值
应用版本（app version）	v1.0.1
数据库 schema 版本（schema_version）	2
迁移脚本目录	server/migrations
迁移引擎位置	server/src/db/migrator.*


⸻

🎯 设计目标

TaskHub 的数据库迁移系统遵循以下原则：
	1.	自动化：无需手动 SQL，程序启动时自动升级。
	2.	幂等性：重复执行不会破坏数据。
	3.	可扩展：每次新版本只需新增一个 SQL 文件。
	4.	无需破坏旧库：旧版本数据库能被自动检测并升级。
	5.	版本完全自描述：所有 schema 版本写入：taskhub_meta。

⸻

📁 文件结构规范

server/
  migrations/
    001_v1_0_0_init.sql
    002_v1_0_1_add_task_fields.sql
  src/
    db/
      migrator.h
      migrator.cpp


⸻

📌 migration file 命名规范

迁移 SQL 文件必须满足以下格式：

NNN_<app_version>_<desc>.sql

示例：

文件名	描述
001_v1_0_0_init.sql	初始化数据库结构
002_v1_0_1_add_task_fields.sql	增加任务的 timeout / retry 字段

规则说明：
	•	NNN：严格递增（从 001 开始）
	•	app_version：对应功能版本
	•	desc：下划线描述

⸻

🧱 当前所有迁移脚本说明

001_v1_0_0_init.sql — 初始化任务系统（schema_version = 1）

功能：
	•	创建 tasks 表（基础字段）
	•	创建 users 表
	•	创建 taskhub_meta 表（数据库版本信息）
	•	写入 meta：schema_version = 1, app_version = v1.0.0

适用场景：
	•	程序首次启动时 tasks 表不存在 → 使用该脚本创建新库

⸻

002_v1_0_1_add_task_fields.sql — 增加任务高级字段（schema_version = 2）

本脚本为 v1.0.1 对任务系统增强所需字段：

新增字段：

timeout_sec
max_retries
retry_count
cancel_flag

修改行为：
	•	创建这些字段并设置默认值
	•	更新 taskhub_meta → schema_version = 2, app_version = v1.0.1

适用场景：
	•	升级旧库（从 v1.0.0 → v1.0.1）

⸻

⚙️ 迁移引擎工作流程（DbMigrator）

迁移逻辑入口：

DbMigrator::migrate(sqlite3* db, "./migrations")

流程分为三大步骤：

⸻

1. 读取当前数据库版本

通过 taskhub_meta 判断数据库是否已经带版本：
	•	存在 → 获取 schema_version
	•	不存在 → 进入推断流程

⸻

2. 旧数据库版本推断（infer）

当数据库缺少 taskhub_meta 时，系统会自动根据现有字段判断库的版本：

判断规则如下：

条件	推断版本
tasks 不存在	schema_version = 0（全新 DB）
tasks 存在但缺少新增字段	schema_version = 1
tasks 存在且字段完整	schema_version = 2

推断完成后会：
	•	自动创建 taskhub_meta
	•	写入推断得到的版本号

⸻

3. 自动执行迁移脚本

伪代码：

for version in (db_version + 1) ... PROGRAM_SCHEMA_VERSION:
    执行 migrations/00X_xxx.sql

示例：

当前版本	目标版本	执行脚本
0	2	init + add_task_fields
1	2	add_task_fields
2	2	不执行脚本
3+	2	警告：DB 超前程序


⸻

🧪 测试方法（必须执行）

下面三个测试确保迁移系统真实可用。

⸻

✔️ 测试 A：新库场景测试
	1.	删除库：

rm taskhub.db

	2.	启动后台
	3.	检查：

sqlite3 taskhub.db
.schema tasks
select * from taskhub_meta;

期待输出：
	•	schema_version = 2
	•	app_version = v1.0.1
	•	tasks 字段齐全

⸻

✔️ 测试 B：旧库（无 meta，但字段完整）
	1.	将备份库复制回来（如 M7 时代的库）：

cp taskhub_backup.db taskhub.db

	2.	运行程序

期待行为：
	•	自动推断为 schema_version = 2
	•	自动创建 taskhub_meta
	•	无需执行任何迁移

⸻

✔️ 测试 C：旧库（无 meta、字段不全）

模拟 v1.0.0：

ALTER TABLE tasks DROP COLUMN timeout_sec;
-- sqlite 不支持 drop column，需重建测试库

运行程序 → 期待：
	•	推断 schema_version = 1
	•	自动执行 002_add_task_fields.sql
	•	升级到 schema_version = 2

⸻

🔧 如何在未来增加新版本？

每当你升级任务系统或数据库结构，你需要做三件事：

第一步：写新的 SQL 脚本

例如：

003_v1_0_2_add_priority.sql

第二步：在 C++ DbMigrator::apply_migrations 中添加 case

if (version == 3) {
    exec_sql_file(db, migrations_dir + "/003_v1_0_2_add_priority.sql");
}

第三步：更新程序 schema 常量

static constexpr int PROGRAM_SCHEMA_VERSION = 3;
static constexpr const char* PROGRAM_APP_VERSION = "v1.0.2";


⸻

📝 总结

TaskHub 的 DB Migration 系统具有：
	•	自动建库
	•	自动推断旧库版本
	•	自动迁移
	•	完整 SQL + 代码迁移机制
	•	支持未来版本扩展
	•	冷启动和升级体验一致
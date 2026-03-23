ALTER TABLE guild_settings ADD COLUMN read_username INTEGER DEFAULT 1;
ALTER TABLE guild_settings ADD COLUMN auto_leave INTEGER DEFAULT 1;
ALTER TABLE guild_settings ADD COLUMN auto_join INTEGER DEFAULT 0;
ALTER TABLE guild_settings ADD COLUMN notify_vc_join INTEGER DEFAULT 0;
ALTER TABLE guild_settings ADD COLUMN max_queue INTEGER DEFAULT 20;
ALTER TABLE guild_settings ADD COLUMN ignore_prefix TEXT DEFAULT '!,/,(,[';

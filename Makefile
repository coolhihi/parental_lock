#---------------------------------------------------------------------------------
# parental_lock - Top-level Build Script
#---------------------------------------------------------------------------------
.PHONY: all sysmodule settings clean install

all: sysmodule settings

sysmodule:
	@echo "=== Building sysmodule ==="
	@$(MAKE) -C sysmodule

settings:
	@echo "=== Building settings app ==="
	@$(MAKE) -C overlay

clean:
	@$(MAKE) -C sysmodule clean
	@$(MAKE) -C overlay clean
	@rm -rf dist

# 打包到 dist/ 目录，可直接复制到SD卡根目录
TITLE_ID := 420000000000CAFE

install: all
	@echo "=== Packaging for SD card ==="
	@mkdir -p dist/atmosphere/contents/$(TITLE_ID)/flags
	@mkdir -p dist/config/parental_lock
	@mkdir -p dist/switch/parental_lock
	@cp sysmodule/parental_lock.nsp dist/atmosphere/contents/$(TITLE_ID)/exefs.nsp
	@touch dist/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag
	@echo '{"name":"parental_lock","tid":"$(TITLE_ID)","requires_reboot":true}' > dist/atmosphere/contents/$(TITLE_ID)/toolbox.json
	@cp overlay/parental_lock_settings.nro dist/switch/parental_lock/
	@echo ""
	@echo "=== Build Complete ==="
	@echo ""
	@echo "SD card layout:"
	@echo "  atmosphere/contents/$(TITLE_ID)/"
	@echo "    exefs.nsp         <- sysmodule"
	@echo "    flags/boot2.flag  <- auto-start at boot"
	@echo "    toolbox.json      <- metadata"
	@echo "  switch/parental_lock/"
	@echo "    parental_lock_settings.nro"
	@echo ""
	@echo "Default password: UUDDLLRR"
	@echo "Default lock time: 30 minutes"

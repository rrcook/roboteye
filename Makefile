# Makefile for RobotEye - Mac OS X 10.4 / PowerPC, gcc 4.0 (Xcode 2.5 tools)
#
#   make            build RobotEye.app
#   make run        build and launch it
#   make preview    render robot_eye_preview.png / robot_eye_poses.png
#   make clean
#
# Overrides, e.g.:  make ARCHFLAGS="-arch ppc -arch i386"

APP       = RobotEye.app
EXE       = RobotEye
BUILD     = build

CC        = gcc
ARCHFLAGS =
MINVER    = -mmacosx-version-min=10.4
CFLAGS    = -O2 -Wall $(ARCHFLAGS) $(MINVER)
LDFLAGS   = $(ARCHFLAGS) $(MINVER)

APP_OBJS  = $(BUILD)/main.o $(BUILD)/EyeView.o $(BUILD)/eye_draw.o
FRAMEWORKS = -framework Cocoa -framework Carbon

all: $(APP)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.m eye_draw.h EyeView.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c eye_draw.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(EXE): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(FRAMEWORKS)

$(APP): $(BUILD)/$(EXE) Info.plist
	rm -rf $(APP)
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources
	cp $(BUILD)/$(EXE) $(APP)/Contents/MacOS/$(EXE)
	cp Info.plist $(APP)/Contents/Info.plist
	printf 'APPL????' > $(APP)/Contents/PkgInfo
	@echo "Built $(APP)"

run: $(APP)
	open $(APP)

$(BUILD)/preview: tools/preview.c eye_draw.c eye_draw.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/preview.c eye_draw.c -framework ApplicationServices

preview: $(BUILD)/preview
	$(BUILD)/preview robot_eye_preview.png robot_eye_poses.png

clean:
	rm -rf $(BUILD) $(APP)

.PHONY: all run preview clean

## Standard things
#
.SUFFIXES:
CC 		:= clang++
BREW		:= $(shell brew --prefix 2>/dev/null || echo $(HOME)/.brew)
CFLAGS		:= -I. -I $(BREW)/include -Wall -Wextra -Werror -g -std='c++14'
LFLAGS		:= -L $(BREW)/lib -lportaudiocpp -lportaudio
RM		:= rm -f
OBJECT_DIR	:= obj
COMP		:= $(CC) $(CFLAGS) -c -o
ECHO		:= echo
#

## Sources directories
#
TEST_DIRS	:= tests
SRC_DIRS	:= src src/cpu utils src/sound src/sound/portaudio
#

## Colors
#
# Hold a real ESC byte rather than the "\033" spelling: recipes run under
# /bin/sh, and whether its echo expands escapes (and accepts -e) varies by
# platform. A literal byte needs neither.
ESC		:= $(shell printf '\033')
BLUE		:= "$(ESC)[34m"
GREEN		:= "$(ESC)[32m"
RED		:= "$(ESC)[31m"
RESET		:= "$(ESC)[0m"
PNAME		:= $(BLUE)$(NAME)$(RESET)
#

## Including sources files
#
include $(patsubst %, %/Sources.mk, $(SRC_DIRS))
#

OBJ_DIRS	:= $(patsubst %, %/obj, $(SRC_DIRS))

.PHONY: all
all: $(OBJ_DIRS) $(OBJECTS) $(IMPL_OBJS)

## Including compilation rules
#
include $(patsubst %, %/Rules.mk, $(SRC_DIRS))
#

%/$(OBJECT_DIR):
	mkdir $@

## Headless ROM debugger (no Qt)
#
DBG_NAME	:= gbmu-dbg

.PHONY: dbg
dbg: $(DBG_NAME)

$(DBG_NAME): all src/$(OBJECT_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ src/$(OBJECT_DIR)/main.o \
		$(filter-out src/$(OBJECT_DIR)/main.o, $(OBJECTS)) \
		$(LFLAGS) -lboost_serialization

# Smoke test: drives gbmu-dbg over a generated ROM and checks every exit code.
.PHONY: test_dbg
test_dbg: $(DBG_NAME)
	@python3 tools/gen_test_rom.py .dbg_pass.gb Passed
	@python3 tools/gen_test_rom.py .dbg_fail.gb Failed
	@fail=0; \
	check() { \
		out=$$(./$(DBG_NAME) $$2 2>/dev/null); rc=$$?; \
		if [ "$$rc" = "$$3" ] && echo "$$out" | grep -q "$$4"; then \
			$(ECHO) "["$(GREEN)OK$(RESET)"] - $$1"; \
		else \
			$(ECHO) "["$(RED)KO$(RESET)"] - $$1 (rc=$$rc)"; echo "$$out"; fail=1; \
		fi; }; \
	check "serial passed"  ".dbg_pass.gb -n 5000000" 0 "serial: passed"; \
	check "serial failed"  ".dbg_fail.gb -n 5000000" 1 "serial: failed"; \
	check "step limit"     ".dbg_pass.gb -n 10"      2 "step limit"; \
	check "breakpoint"     ".dbg_pass.gb -b 150"     0 "breakpoint"; \
	check "trace"          ".dbg_pass.gb -n 1 -t"    2 "^0000  31 FE FF  LD SP d16$$"; \
	check "memory dump"    ".dbg_pass.gb -b 150 -d 0161" 0 "^0161  50 61 73 73 65 64"; \
	check "bad rom"        "/dev/null"               1 ""; \
	check "frame limit"    ".dbg_pass.gb --frames 3"  2 "frame limit"; \
	check "screenshot"     ".dbg_pass.gb --frames 3 -s .dbg_shot.ppm" 2 "frame limit"; \
	sz=$$(wc -c < .dbg_shot.ppm | tr -d ' '); \
	if [ "$$sz" = "69135" ]; then \
		$(ECHO) "["$(GREEN)OK$(RESET)"] - screenshot is a 160x144 ppm"; \
	else \
		$(ECHO) "["$(RED)KO$(RESET)"] - screenshot is a 160x144 ppm ($$sz bytes)"; fail=1; \
	fi; \
	$(RM) .dbg_pass.gb .dbg_fail.gb .dbg_shot.ppm; \
	exit $$fail
#

## Including tests
#
-include $(patsubst %, %/Rules.mk, $(TEST_DIRS))
#

.PHONY: clean
clean:
	@$(RM) -r $(OBJ_DIRS)
	@$(ECHO) "Objects directories removed"

.PHONY: fclean
fclean: clean
	@$(RM) $(TEST_TARGETS) $(DBG_NAME)
	@$(ECHO) $(NAME) "deleted"

.PHONY: re
re: fclean all

.PHONY: makedeps
makedeps:
	python3 ./tools/gen_make_sources.py --source='.cpp' $(SRC_DIRS)

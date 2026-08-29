import argparse
import glob
import os

SOURCES_CONTENT = \
"""## Auto generated make tool, don't edit manually.

OBJ_FILES_{0} := {1}
OBJECTS{2} += $(patsubst %, {0}/$(OBJECT_DIR)/%, $(OBJ_FILES_{0}))"""

# Header dependencies are not written here. The compiler emits a .d file beside
# every object (see -MMD -MP in RawProject.mk), which RawProject.mk includes.
# This file used to list them, and silently missed several objects.
RULES_CONTENT = \
"""## Auto generated make tool, don't edit manually.
{0}/$(OBJECT_DIR)/%.o: {0}/%{1}
	$(COMP) $@ $<
"""

arg_parser = argparse.ArgumentParser(description='Generate Makefile utility files')
arg_parser.add_argument('-e', '--exec', help='Specify the exec_file')
arg_parser.add_argument('-s', '--source', help='Specify the source extension', default='.c');
arg_parser.add_argument('directories', metavar='Dir', nargs='+',
                    help='Directories containing sources files')
args = arg_parser.parse_args()

def format_c_file(file, padding):
    file = ' ' * padding + os.path.basename(file)
    return file.replace(args.source, '.o \\')

def write_sources(dir, objects):
    sources = open(os.path.join(dir, 'Sources.mk'), 'w')
    if args.exec:
        sources.write(SOURCES_CONTENT.format(dir, objects, '_' + args.exec.upper()))
    else:
        sources.write(SOURCES_CONTENT.format(dir, objects, ''))
    sources.close()

def write_rules(dir):
    deps_file = open(os.path.join(dir, 'Rules.mk'), 'w')
    deps_file.write(RULES_CONTENT.format(dir, args.source))
    deps_file.close()

for dir in args.directories:
    objects = None
    extension = '*' + args.source
    for file in sorted(glob.glob(os.path.join(dir, extension))):
        if objects is None:
            objects = format_c_file(file, 0) + '\n'
        else:
            objects += format_c_file(file, len('OBJ_FILES_' +  dir + ' := ')) + '\n'
    if objects is None:
        objects = ''
    objects = objects[:len(objects) - 3] + '\n'
    write_sources(dir, objects)
    write_rules(dir)

#!/usr/bin/env python3
import json
import sys
import argparse

colors = {
    'reset':   '\x1B[0m',
    'black':   '\x1B[30m',
    'red':     '\x1B[31m',
    'green':   '\x1B[32m',
    'yellow':  '\x1B[33m',
    'blue':    '\x1B[34m',
    'magenta': '\x1B[35m',
    'cyan':    '\x1B[36m',
    'white':   '\x1B[37m'
}

def colored_print(input, color = 'reset', file = sys.stdout, end = '\n'):
    if file.isatty():
        print("{}{}{}".format(colors[color], input, colors['reset']), file = file, end = end)
    else:
        print("{}".format(input), file = file, end = end)

argument_parser = argparse.ArgumentParser()
argument_parser.add_argument('old_file')
argument_parser.add_argument('new_file')
argument_parser.add_argument('-d', '--detailed',
                    help = 'Perform a detailed diff, including size and mode' + \
                    ' parameters which may change over time',
                    action = 'store_true')
args = argument_parser.parse_args()

old_data = None
new_data = None

with open(args.old_file, 'r') as old_file:
    old_data = json.loads(old_file.read())

with open(args.new_file, 'r') as new_file:
    new_data = json.loads(new_file.read())

if len(old_data) != len(new_data):
    colored_print('=== ENTRIES COUNT MISMATCH ===')
    colored_print(f'old: {len(old_data)}', color = 'red')
    colored_print(f'new: {len(new_data)}', color = 'green')

new_pids_set = {(i['p'], i['x'], i['e'], i['b'], i['w']) for i in new_data}
old_pids_set = {(i['p'], i['x'], i['e'], i['b'], i['w']) for i in old_data}

if new_pids_set != old_pids_set:
    new_pids_diff = new_pids_set.difference(old_pids_set)
    old_pids_diff = old_pids_set.difference(new_pids_set)
    colored_print('=== PID SET MISMATCH ===')
    for pid in new_pids_diff:
        colored_print(f'+\t{{\"p\": {pid[0]}, \"x\": {pid[1]}, \"e\": {pid[2]}, \"b\": \"{pid[3]}\", \"w\": \"{pid[4]}\"}}', color = 'green')
    for pid in old_pids_diff:
        colored_print(f'-\t{{\"p\": {pid[0]}, \"x\": {pid[1]}, \"e\": {pid[2]}, \"b\": \"{pid[3]}\", \"w\": \"{pid[4]}\"}}', color = 'red')
    sys.exit(0)

old_data.sort(key = lambda x: x['p'])
new_data.sort(key = lambda x: x['p'])

for execution in old_data:
    if len(execution['o']) > 0:
        execution['o'].sort(key = lambda x: x['p'])
    if len(execution['c']) > 0:
        execution['c'].sort(key = lambda x: x['p'])

for execution in new_data:
    if len(execution['o']) > 0:
        execution['o'].sort(key = lambda x: x['p'])
    if len(execution['c']) > 0:
        execution['c'].sort(key = lambda x: x['p'])

for old_entry, new_entry in zip(old_data, new_data):
    old_children = {(i['p'], i['f']) for i in old_entry['c']}
    new_children = {(i['p'], i['f']) for i in new_entry['c']}

    if old_children != new_children:
        old_children_diff = old_children.difference(new_children)
        new_children_diff = new_children.difference(old_children)
        colored_print('=== ({}, {}) ===\n\t"c": ['.format(new_entry['p'], new_entry['x']))
        for child in new_children_diff:
            colored_print(f'+\t\t{{ "p": {child[0]}, "f": {child[1]} }}', color = 'green')
        for child in old_children_diff:
            colored_print(f'-\t\t{{ "p": {child[0]}, "f": {child[1]} }}', color = 'red')
        colored_print('\t]')

    old_arguments = {i for i in old_entry['v']}
    new_arguments = {i for i in new_entry['v']}

    if old_arguments != new_arguments:
        old_arguments_diff = old_arguments.difference(new_arguments)
        new_arguments_diff = new_arguments.difference(old_arguments)
        colored_print('=== ({}, {}) ===\n\t"v": ['.format(new_entry['p'], new_entry['x']))
        for argument in new_arguments_diff:
            colored_print('+\t\t\"{}\"'.format(argument), color = 'green')
        for argument in old_arguments_diff:
            colored_print('-\t\t\"{}\"'.format(argument), color = 'red')
        colored_print('\t]')

    # Not every file entry will have 'o', so we have to make the comprehension more specific.
    # We also don't really care about other fields than the paths because the mode/size can be undefined.
    # @TODO: Add checking mode and size?
    old_files = set()
    new_files = set()
    for file in old_entry['o']:
        original_path = file.get('o')
        if args.detailed is True:
            old_files.add((file['p'], original_path, file['m'], file['s']))
        else:
            old_files.add((file['p'], original_path))

    for file in new_entry['o']:
        original_path = file.get('o')
        if args.detailed is True:
            new_files.add((file['p'], original_path, file['m'], file['s']))
        else:
            new_files.add((file['p'], original_path))

    if old_files != new_files:
        old_files_diff = old_files.difference(new_files)
        new_files_diff = new_files.difference(old_files)
        colored_print('=== ({}, {}) ===\n\t"o": ['.format(new_entry['p'], new_entry['x']))
        for file in new_files_diff:
            if args.detailed is True:
                colored_print(f'+\t\t{{ \"p\": \"{file[0]}\", \"o\": \"{file[1]}\", \"m\": {file[2]}, \"s\": {file[3]} }}', color = 'green')
            else:
                colored_print(f'+\t\t{{ \"p\": \"{file[0]}\", \"o\": \"{file[1]}\" }}', color = 'green')
        for file in old_files_diff:
            if args.detailed is True:
                colored_print(f'-\t\t{{ \"p\": \"{file[0]}\", \"o\": \"{file[1]}\", \"m\": {file[2]}, \"s\": {file[3]} }}', color = 'red')
            else:
                colored_print(f'-\t\t{{ \"p\": \"{file[0]}\", \"o\": \"{file[1]}\" }}', color = 'red')
        colored_print('\t]')

    old_endpoints = {(i['p'], i['x']) for i in old_entry['i']}
    new_endpoints = {(i['p'], i['x']) for i in new_entry['i']}

    if old_endpoints != new_endpoints:
        old_endpoints_diff = old_endpoints.difference(new_endpoints)
        new_endpoints_diff = new_endpoints.difference(old_endpoints)
        colored_print('=== ({}, {})===\n\t"i": ['.format(new_entry['p'], new_entry['x']))
        for endpoint in new_endpoints_diff:
            colored_print('+\t\t{{ "p": {}, "x": {} }}'.format(endpoint[0], endpoint[1]), color = 'green')
        for endpoint in old_endpoints_diff:
            colored_print('-\t\t{{ \"p\": {}, \"x\": {} }}'.format(endpoint[0], endpoint[1]), color = 'red')
        colored_print('\t]')

    if old_entry['r'] != new_entry['r']:
        old_parent = old_entry['r']
        new_parent = new_entry['r']
        colored_print('=== ({}, {}) ===\n'.format(new_entry['p'], new_entry['x']))
        colored_print('+\t"r": {{ "p": {}, "x": {} }}'.format(new_parent['p'], new_parent['x']), color = 'green')
        colored_print('-\t"r": {{ "p": {}, "x": {} }}'.format(old_parent['p'], old_parent['x']), color = 'red')

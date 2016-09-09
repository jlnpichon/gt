#!/usr/bin/env python
import os
import subprocess
import sys

def usage():
    print '{0} <command> [<args>]'.format(sys.argv[0])
    print '\nCommands list:'
    print '    add'
    print '    commit'

def binary_check():
    if not (os.path.isfile('./update-index') and
            os.path.isfile('./write-tree') and
            os.path.isfile('./commit-tree')):
        print 'build gt before ($make)'
        sys.exit(1)

def add(args):
    if len(args) < 1:
        print 'missing filename'
        print '{0} add <file>...'.format(sys.argv[0])
        sys.exit(1)

    result = subprocess.Popen(['./update-index', '--add', '--'] + args,
            stdout=subprocess.PIPE)
    out, err = result.communicate()
    sys.exit(result.returncode)

def commit(args):
    result = subprocess.Popen(['./write-tree'], stdout=subprocess.PIPE)
    out, err = result.communicate()
    if result.returncode != 0:
        sys.exit(result.returncode)

    tree_sha1 = out.replace("\n", "")

    parent_sha1 = None
    head_file = os.getenv('GT_DIRECTORY') or '.gt' + '/HEAD'
    try:
        f = open(head_file, 'r')
        parent_sha1 = f.readline()
        f.close()
    except:
        pass

    cmd = ['./commit-tree', tree_sha1]
    if parent_sha1 is not None and parent_sha1 != '':
        cmd = cmd + ['--parent', parent_sha1]

    result = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    out, err = result.communicate()
    if result.returncode != 0:
        sys.exit(result.returncode)

    commit_sha1 = out.replace("\n", "")

    f = open(head_file, 'w')
    f.write(commit_sha1)
    f.close()

commands = {'add': add, 'commit': commit}

def command_execute(args):
    command = args[0]
    if not command in commands:
        print 'Unknown command \'{0}\''.format(command)
        usage()
        sys.exit(1)

    fn = commands[command]
    fn(args[1:])

if __name__ == '__main__':
    if len(sys.argv) < 2:
        usage()
        sys.exit(1)

    binary_check()
    command_execute(sys.argv[1:])

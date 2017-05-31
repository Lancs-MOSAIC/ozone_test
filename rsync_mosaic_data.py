#!/usr/bin/env python

import argparse
import datetime
import logging
import numpy as np
import os
import subprocess
import sys


logger = logging.getLogger(__name__)

if __name__ == '__main__':
    # Parse command line arguments
    now64 = np.datetime64('now', 's')
    progname = os.path.basename(sys.argv[0]).partition('.')[0]
    default_config_file = os.path.join(os.path.sep, 'etc', 'camera.ini')

    remote_host = 'awn-data'

    parser = argparse.ArgumentParser(description='Transfer camera images')
    parser.add_argument('--log-level',
                        choices=['debug', 'info', 'warning',
                                 'error', 'critical'],
                        default='info',
                        help='Control how much detail is printed',
                        metavar='LEVEL')
    parser.add_argument('-n', '--dry-run',
                        action='store_true',
                        help='Dry run')
    parser.add_argument('--local-directory',
                        default='/home/ozone/data',
                        help='Local directory for rsync')
    parser.add_argument('--remote-directory',
                        default='',
                        help='Remote directory for rsync')
    # For --remote-host it is assumed that the .ssh/config file will
    # define an entry for mosaic-data.
    parser.add_argument('--remote-host',
                        default='mosaic-data',
                        help='Remote host for rsync')
    parser.add_argument('--remove-source-files',
                        action='store_true',
                        help='Remove source files')
    parser.add_argument('-v', '--verbose',
                        action='append_const',
                        const='-v',
                        help='Be verbose')
    args = parser.parse_args()

    logger = logging.getLogger(__name__)
    logging.basicConfig(level=getattr(logging, args.log_level.upper()))


    # Get station number
    spec_file = '/home/ozone/mosaic/ozone_test/ozonespec.conf'
    #if not os.path.exists(spec_file):
    #    raise Exception('missing ' + spec_file)

    vrstnum = None
    with open(spec_file) as fh:
        for s in fh.readlines():
            print(s)
        
    rsync = ['rsync']
        
    if args.verbose:
        rsync.extend(args.verbose)

    if args.dry_run:
        rsync.append('--dry-run')
        
    # t = st
    # while t < et:
    #     logger.debug('{0:%Y-%m-%d}'.format(t))
    #     files = set()
    #     files_to_rsync = set()
    #     for g in config.get('upload', 'files').split():
    #         for f in glob.glob(g.format(DateTime=t)):
    #             files.add(f)
    #             if os.path.exists(f):
    #                 files_to_rsync.add(f)

    #     if len(files_to_rsync):
    #         logger.debug('files: ' + ', '.join(files))
    #         cmd = []
    #         cmd.extend(rsync)
    #         if args.remove_source_files:
    #             cmd.append('--remove-source-files')

    #         cmd.extend(files_to_rsync)
    #         cmd.append(remote_host + ':' + remote_directory)
    #         logger.info('running %s', ' '.join(cmd))
    #         subprocess.check_call(cmd)
    #     else:
    #         logger.debug('no files to transfer')

    #     if args.remove_source_files:
    #         dirs_to_del = set()
    #         for f in sorted(files):
    #             for p in get_intermediate_dirs(f):
    #                 if os.path.isdir(p):
    #                     try:
    #                         os.rmdir(p)  # Will succeed only when empty
    #                         logger.info('removed directory %s', p)
    #                     except:
    #                         pass

    #     t += interval


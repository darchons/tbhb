#!/usr/bin/env python2
# Copyright 2013, Jim Chen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import array, ctypes, fcntl, os, struct, subprocess, sys
from collections import namedtuple

class _IOC:

    IOC_READ       = 2
    IOC_WRITE      = 1
    IOC_SIZEBITS   = 14

    @classmethod
    def get_ioc(cls, inout, group, num, length):
        return (inout << (cls.IOC_SIZEBITS + 16)) | (length << 16) | (group << 8) | num

class SPIDev(object):
    '''An SPI device as exposed through the kernel SPI driver'''

    SPI_CPHA = 0x01
    SPI_CPOL = 0x02

    SPI_MODE_0 = (0|0)
    SPI_MODE_1 = (0|SPI_CPHA)
    SPI_MODE_2 = (SPI_CPOL|0)
    SPI_MODE_3 = (SPI_CPOL|SPI_CPHA)

    SPI_CS_HIGH   = 0x04
    SPI_LSB_FIRST = 0x08
    SPI_3WIRE     = 0x10
    SPI_LOOP      = 0x20
    SPI_NO_CS     = 0x40
    SPI_READY     = 0x80

    SPI_IOC_MAGIC = ord('k')

    Options = namedtuple('Options', [
        'speed_hz',
        'delay_usecs',
        'bits_per_word',
        'cs_change'])

    Transfer = namedtuple('Transfer', [
        'data',
        'options'])

    _SPI_IOC_TRANSFER_SIZE = 32

    _SPI_IOC_RD_MODE = _IOC.get_ioc(_IOC.IOC_READ, SPI_IOC_MAGIC, 1, 1)
    _SPI_IOC_WR_MODE = _IOC.get_ioc(_IOC.IOC_WRITE, SPI_IOC_MAGIC, 1, 1)

    _SPI_IOC_RD_LSB_FIRST = _IOC.get_ioc(_IOC.IOC_READ, SPI_IOC_MAGIC, 2, 1)
    _SPI_IOC_WR_LSB_FIRST = _IOC.get_ioc(_IOC.IOC_WRITE, SPI_IOC_MAGIC, 2, 1)

    _SPI_IOC_RD_BITS_PER_WORD = _IOC.get_ioc(_IOC.IOC_READ, SPI_IOC_MAGIC, 3, 1)
    _SPI_IOC_WR_BITS_PER_WORD = _IOC.get_ioc(_IOC.IOC_WRITE, SPI_IOC_MAGIC, 3, 1)
    BITS_PER_WORD_MIN = 4
    BITS_PER_WORD_MAX = 32

    _SPI_IOC_RD_MAX_SPEED_HZ = _IOC.get_ioc(_IOC.IOC_READ, SPI_IOC_MAGIC, 4, 4)
    _SPI_IOC_WR_MAX_SPEED_HZ = _IOC.get_ioc(_IOC.IOC_WRITE, SPI_IOC_MAGIC, 4, 4)

    @classmethod
    def _get_msg_size(cls, n):
        return (n * cls._SPI_IOC_TRANSFER_SIZE
            if n * cls._SPI_IOC_TRANSFER_SIZE < (1 << _IOC.IOC_SIZEBITS) else 0)

    @classmethod
    def _get_spi_ioc_message(cls, n):
        return _IOC.get_ioc(_IOC.IOC_WRITE, cls.SPI_IOC_MAGIC, 0, cls._get_msg_size(n))

    @classmethod
    def install(cls, verbose=False):
        '''Install SPI support by stopping blacklisting the SPI driver
            and changing SPI devices to be world-accessible.
        '''
        spidriver = 'spi-bcm2708'
        confdir = os.path.sep.join(['', 'etc', 'modprobe.d'])
        if os.path.isdir(confdir):
            if verbose:
                print >> sys.stderr, 'Looking for blacklists under %s' % confdir
            for conf in os.listdir(confdir):
                conf = os.path.join(confdir, conf)
                if not os.path.isfile(conf):
                    continue
                with open(conf, 'r') as fconf:
                    lines = fconf.readlines()
                    if not any(('blacklist' in l and 'spi' in l and '#' not in l)
                        for l in lines):
                        continue
                with open(conf, 'w') as fconf:
                    lines = ['# ' + l if ('blacklist' in l and 'spi' in l and '#' not in l)
                        else l for l in lines]
                    fconf.writelines(lines)
                    if verbose:
                        print >> sys.stderr, 'Removed blacklist in file %s' % conf
                    break
        elif verbose:
            print >> sys.stderr, 'Skipped checking blacklist'

        hasRule = False
        rulesdir = os.path.sep.join(['', 'etc', 'udev', 'rules.d'])
        if os.path.isdir(confdir):
            if verbose:
                print >> sys.stderr, 'Looking for udev rules under %s' % rulesdir
            for rules in os.listdir(rulesdir):
                rules = os.path.join(rulesdir, rules)
                if not os.path.isfile(rules):
                    continue
                with open(rules, 'r') as frules:
                    lines = frules.readlines()
                    if any('spidev' in l for l in lines):
                        if verbose:
                            print >> sys.stderr, 'Found rules in %s' % rules
                        hasRules = True
                        break
            if not hasRule:
                rules = os.path.join(rulesdir, '50-spidev.rules')
                with open(rules, 'w') as frules:
                    frules.write(
                        'KERNEL=="spidev*", '
                        'MODE="0666"\n')
                    if verbose:
                        print >> sys.stderr, 'Created rules in %s' % rules
        elif verbose:
            print >> sys.stderr, 'Skipped checking udev rules'

        if verbose:
            print >> sys.stderr, 'Restarting %s driver' % spidriver
        subprocess.check_call(['modprobe', '-r', spidriver])
        subprocess.check_call(['modprobe', spidriver])
        if verbose:
            print >> sys.stderr, 'Restarted %s driver' % spidriver

    @classmethod
    def get_devices(cls):
        '''Return a list of SPI devices present on the system.
            Each device is given as the full path useful as the 'dev'
            argument when creating SPIDev()
        '''
        devices = []
        devpath = os.path.sep + 'dev'
        for dev in os.listdir(devpath):
            devfull = os.path.join(devpath, dev)
            if dev.startswith('spidev') and os.path.exists(devfull):
                devices.append(devfull)
        return devices

    def __init__(self, dev, mode=None, bits_per_word=None,
                 words_per_transfer=None, max_speed_hz=None, lsb_first=None):
        '''Create a new object representing an SPI device,
            and optionally open the device
        '''
        self._mode = self.SPI_MODE_0 if mode is None else mode
        self._bits_per_word = 8 if bits_per_word is None else bits_per_word
        self._words_per_transfer = (1 if words_per_transfer
            is None else words_per_transfer)
        self._max_speed_hz = max_speed_hz
        self._lsb_first = False if lsb_first is None else lsb_first

        self._dev = dev
        self._file = None
        self._open()

    @property
    def device(self):
        '''Return the full path of the device.
        '''
        return self._dev

    def _open(self):
        self._checkOpen(isOpen=False)

        if not self._dev:
            raise ValueError('Device not specified.')
        devpath = os.path.sep + 'dev'
        if not os.path.exists(self._dev):
            if self._dev.startswith('spidev'):
                self._dev = os.path.join(devpath, self._dev)
            elif self._dev[0].isdigit():
                self._dev = os.path.join(devpath, 'spidev' + self._dev)

        self._dev = os.path.abspath(self._dev)
        self._file = open(self._dev, 'r+')

        # set initial options
        self.mode = self._mode
        self.bits_per_word = self._bits_per_word
        self.words_per_transfer = self._words_per_transfer
        self.max_speed_hz = self._max_speed_hz
        self.lsb_first = self._lsb_first

    def _getProp(self, name, fmt, rd_ioctl):
        self._checkOpen()
        data = struct.pack(fmt, 0)
        data = fcntl.ioctl(self._file, rd_ioctl, data)
        value = struct.unpack(fmt, data)
        setattr(self, '_%s' % name, value)
        return value

    def _setProp(self, name, fmt, value, minimum, maximum, wr_ioctl):
        self._checkOpen()
        if value is None:
            return
        if (not isinstance(value, (int, long)) or
            value < minimum or value > maximum):
            raise ValueError('invalid %s' % name)
        data = struct.pack(fmt, value)
        fcntl.ioctl(self._file, wr_ioctl, data)
        setattr(self, '_%s' % name, value)

    @property
    def mode(self):
        '''Get the current SPI mode
        '''
        return self._getProp('mode', '=B', self._SPI_IOC_RD_MODE)

    @mode.setter
    def mode(self, value):
        '''Set the SPI mode using the SPI_CPHA/SPI_CPOL/SPI_MODE_ constants
        '''
        self._setProp('mode', '=B', value, 0, 255, self._SPI_IOC_WR_MODE)

    @property
    def bits_per_word(self):
        '''Get the current bits per word
        '''
        return self._getProp('bits_per_word', '=B',
            self._SPI_IOC_RD_BITS_PER_WORD)

    @bits_per_word.setter
    def bits_per_word(self, value):
        '''Set the bits per word from 4 to 32
        '''
        self._setProp('bits_per_word', '=B', value,
            self.BITS_PER_WORD_MIN, self.BITS_PER_WORD_MAX,
            self._SPI_IOC_WR_BITS_PER_WORD)

    @property
    def words_per_transfer(self):
        '''Get the default words per transfer
        '''
        self._checkOpen()
        return self._words_per_transfer

    @words_per_transfer.setter
    def words_per_transfer(self, value):
        '''Set the default words per transfer
        '''
        self._checkOpen()
        self._words_per_transfer = value

    @property
    def max_speed_hz(self):
        '''Get the current maximum speed in hertz
        '''
        return self._getProp('max_speed_hz', '=L',
            self._SPI_IOC_RD_MAX_SPEED_HZ)

    @max_speed_hz.setter
    def max_speed_hz(self, value):
        '''Set the maximum speed in hertz from 1kHz to 1GHz
        '''
        self._setProp('max_speed_hz', '=L', value, 1e3, 1e9,
            self._SPI_IOC_WR_MAX_SPEED_HZ)

    @property
    def lsb_first(self):
        '''Get the current least-significant-bit-first setting
        '''
        return self._getProp('lsb_first', '=B',
            self._SPI_IOC_RD_LSB_FIRST)

    @lsb_first.setter
    def lsb_first(self, value):
        '''Set whether the least-significant bit is transfered first in a word
        '''
        self._setProp('lsb_first', '=B', 1 if value else 0, 0, 1,
            self._SPI_IOC_WR_LSB_FIRST)

    def close(self):
        '''Close the device and make this device inaccessible
        '''
        self._checkOpen()
        self._file.close()
        self._file = None

    def _checkOpen(self, isOpen=True):
        if isOpen and self._file is None:
            raise IOError('Device not open.')
        if not isOpen and self._file is not None:
            raise IOError('Device already open.')

    def flush(self):
        '''Flush the buffers for this device
        '''
        self._checkOpen()
        self._file.flush()

    def fileno(self):
        '''Return the open file number for this device
        '''
        self._checkOpen()
        return self._file.fileno()

    def next(self):
        '''Perform a transfer and return the words read.
            This method never raises StopIteration
        '''
        return self.read()

    def read(self, size=None, options=None):
        '''Read data; size specifies the number of words, and options
            specify specific SPI options used for this transfer. Default
            word count per transfer is used if size is not specified.
        '''
        self._checkOpen()
        if size is None:
            size = self._words_per_transfer
        if not isinstance(size, (int, long)):
            raise ValueError('invalid size')
        if options is not None:
            return self.access(self.Transfer(size, options))
        # size is words but read expects bytes
        return self._file.read(size * ((self._bits_per_word + 7) // 8))

    def write(self, data):
        '''Write data; data can be any format accepted by the file.write
            method, or an object of type SPIDev.Transfer
        '''
        self._checkOpen()
        if isinstance(data, self.Transfer):
            self.access(data)
            return
        self._file.write(data)

    def access(self, data):
        '''Read and write data; data can be in the following formats,
            * integer - write the specified number of all-zero words
            * string - write words converted from string
            * tuple/list/array of integers - write words
            * SPIDev.Transfer - write above data types with options
            * tuple/list of the above types - write transfers
            Note that a tuple/list of integers will be interpreted as
            transfers; put it in another tuple/list to interpret them
            as words.

            Read data is returned in the same format as the input data.
            For integer input (i.e. writing N zeros), a string is returned.
            For SPIDev.Transfer input, the inner data format is used.
        '''
        self._checkOpen()

        def getWordType(bpw):
            if (not isinstance(bpw, (int, long)) or
                bpw < self.BITS_PER_WORD_MIN or bpw > self.BITS_PER_WORD_MAX):
                raise 'invalid bits per word'
            return (('B', 1) if bpw <= 8 else
                    ('H', 2) if bpw <= 16 else
                    ('L', 4))

        def makeTransfer(data):
            if isinstance(data, self.Transfer):
                options = data.options
                data = data.data
                wordtype, wordsize = getWordType(options.bits_per_word
                    if options.bits_per_word else self._bits_per_word)
            else:
                options = None
                wordtype, wordsize = getWordType(self._bits_per_word)

            if isinstance(data, int):
                return self.Transfer(data * wordsize, options), str
            elif isinstance(data, str):
                return self.Transfer(data, options), str
            elif isinstance(data, (tuple, list)):
                return self.Transfer(struct.pack('=%d%s' %
                    (len(data), wordtype), *data), options), type(data)
            elif isinstance(data, array.array):
                return self.Transfer(data.tostring(), options), type(data)

            raise ValueError('unrecognized data type')

        if isinstance(data, (tuple, list)):
            msgsize = self._get_msg_size(len(data))
            # fnctl only support 1024 byte messages
            if msgsize == 0 or msgsize > 1024:
                raise ValueError('too many transfers')
            transfers = (makeTransfer(tdata) for tdata in data)
        else:
            transfers = (makeTransfer(data),)

        txbufs = []
        rxbufs = []
        rettypes = []
        wordtypes = []
        msg = ''
        for t, rettype in transfers:
            if isinstance(t.data, (int, long)):
                txbuf = ctypes.create_string_buffer(t.data)
                rxbuf = ctypes.create_string_buffer(t.data)
                ctypes.memset(txbuf, 0, t.data)
                length = t.data
            else:
                txbuf = ctypes.create_string_buffer(t.data, len(t.data))
                rxbuf = ctypes.create_string_buffer(len(t.data))
                length = len(t.data)
            txbufs.append(txbuf)
            rxbufs.append(rxbuf)
            # spi_ioc_transfer structure
            msg += struct.pack('=QQLLHBBL',
                ctypes.addressof(txbuf),
                ctypes.addressof(rxbuf),
                length,
                t.options.speed_hz if t.options else 0,
                t.options.delay_usecs if t.options else 0,
                t.options.bits_per_word if t.options else 0,
                (1 if t.options.cs_change else 0) if t.options else 1,
                0)
            rettypes.append(rettype)
            wordtypes.append(getWordType(t.options.bits_per_word
                if t.options else self._bits_per_word))

        fcntl.ioctl(self._file, self._get_spi_ioc_message(len(transfers)), msg)

        ret = []
        for i, rxbuf in enumerate(rxbufs):
            retstr = rxbuf.raw
            if rettypes[i] is str:
                ret.append(retstr)
                continue
            unpacked = struct.unpack('=%d%s' %
                (len(retstr) // wordtypes[i][1], wordtypes[i][0]), retstr)
            if issubclass(rettypes[i], (tuple, list)):
                ret.append(rettypes[i](unpacked))
                continue
            elif issubclass(rettypes[i], array.array):
                ret.append(rettypes[i](wordtypes[i][0], unpacked))
                continue
            assert False
        if isinstance(data, list):
            return ret
        if isinstance(data, tuple):
            return tuple(ret)
        assert len(ret) == 1
        return ret[0]

    def __iter__(self):
        '''Return object itself as iterator
        '''
        self._checkOpen()
        return self

    def __enter__(self):
        '''Return object itself for use in with statement
        '''
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        '''Close device when exiting with statement
        '''
        if self._file is not None:
            self.close()

if __name__ == '__main__':

    import argparse, struct

    parser = argparse.ArgumentParser(description='Access an SPI device',
        epilog=('Available devices: ' + ', '.join(SPIDev.get_devices())))

    CHOICES = ['install', 'r', 'read', 's', 'string']
    parser.add_argument('device', metavar='DEVICE',
        help='full path of SPI device')
    parser.add_argument('-v', '--verbose', action='store_true',
        help='print extra messages')
    parser.add_argument('-m', '--mode', default=SPIDev.SPI_MODE_0, type=int,
        choices={SPIDev.SPI_MODE_0, SPIDev.SPI_MODE_1,
            SPIDev.SPI_MODE_2, SPIDev.SPI_MODE_3},
        help='SPI mode (default: 0)')
    def bits_per_word(string):
        bpw = int(string, 0)
        if bpw < SPIDev.BITS_PER_WORD_MIN or bpw > SPIDev.BITS_PER_WORD_MAX:
            raise ValueError('bits per word outside of range')
        return bpw
    parser.add_argument('-b', '--bits', default=8, type=bits_per_word,
        help='Bits per word from %d to %d (default: 8)' %
            (SPIDev.BITS_PER_WORD_MIN, SPIDev.BITS_PER_WORD_MAX))
    parser.add_argument('-s', '--speed', metavar='HZ', type=int,
        help='Speed of transfer in hertz')
    parser.add_argument('--lsb-first', action='store_true',
        help='Send least-significant-bit first')
    command_parser = parser.add_subparsers()

    def install(args):
        SPIDev.install(verbose=args.verbose)

    install_parser = command_parser.add_parser('install',
        help='install access to SPI device, '
            'including enabling drivers and setting permissions '
            '(needs root)')
    install_parser.set_defaults(run=install)

    def write(args):
        if args.verbose:
            print >> sys.stderr, 'Opening %s' % args.device
        dev = SPIDev(args.device, mode=args.mode, bits_per_word=args.bits,
            max_speed_hz=args.speed, lsb_first=args.lsb_first)
        if args.verbose:
            print >> sys.stderr, 'Opened file %d' % dev.fileno()

        def to_hex(data):
            n = ((1, 'B') if args.bits <= 8 else
                 (2, 'H') if args.bits <= 16 else
                 (4, 'L'))
            fmt = '%d%s' % (len(data) // n[0], n[1])
            data = struct.unpack('=' + fmt, data)
            data = struct.pack('>' + fmt, *data)
            data = data.encode('hex')
            return ' '.join(data[i: i + 2 * n[0]]
                for i in range(0, len(data), 2 * n[0]))

        def from_hex(data):
            n = ((1, 'B') if args.bits <= 8 else
                 (2, 'H') if args.bits <= 16 else
                 (4, 'L'))
            if len(data) % n[0] != 0:
                raise ValueError('invalid hex word')
            fmt = '%d%s' % (len(data) // n[0], n[1])
            data = struct.unpack('>' + fmt, data)
            data = struct.pack('=' + fmt, *data)
            return data

        if args.action == 'r':
            for c in args.data:
                if args.verbose:
                    print >> sys.stderr, 'Reading %d words' % c
                print to_hex(dev.read(c))
        elif args.action == 'x':
            for x in args.data:
                if args.verbose:
                    print >> sys.stderr, 'Writing/reading %d bytes' % len(x)
                print to_hex(dev.access(from_hex(x)))
        elif args.action == 's':
            for s in args.data:
                if args.verbose:
                    print >> sys.stderr, 'Writing/reading %d bytes' % len(s)
                print dev.access(s)
        else:
            assert False

    def python_int(string):
        return int(string, 0)
    read_parser = command_parser.add_parser('read',
        help='read COUNT numbers of words and '
            'print the data in hex format')
    read_parser.add_argument('data', nargs='+',
        metavar='COUNT', type=python_int,
        help='number of words to read for each transfer')
    read_parser.set_defaults(run=write, action='r')
    read_parser = command_parser.add_parser('r')
    read_parser.add_argument('data', nargs='+',
        metavar='COUNT', type=python_int,
        help='number of words to read for each transfer')
    read_parser.set_defaults(run=write, action='r')

    def hex_string(string):
        return string.decode('hex')
    hex_parser = command_parser.add_parser('hex',
        help='write HEX data and print the read data in hex format')
    hex_parser.add_argument('data', nargs='+', metavar='HEX', type=hex_string,
        help='hex data to write for each transfer')
    hex_parser.set_defaults(run=write, action='x')
    hex_parser = command_parser.add_parser('x')
    hex_parser.add_argument('data', nargs='+', metavar='HEX', type=hex_string,
        help='hex data to write for one transfer')
    hex_parser.set_defaults(run=write, action='x')

    string_parser = command_parser.add_parser('string',
        help='write STRING data and print the read data in string format')
    string_parser.add_argument('data', nargs='+', metavar='STRING',
        help='string data to write for each transfer')
    string_parser.set_defaults(run=write, action='s')
    string_parser = command_parser.add_parser('s')
    string_parser.add_argument('data', nargs='+', metavar='STRING',
        help='string data to write for each transfer')
    string_parser.set_defaults(run=write, action='s')

    if len(sys.argv) < 2:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()
    args.run(args)


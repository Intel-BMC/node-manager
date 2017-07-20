from string import Template
import os
import sys
import glob
import subprocess
import StringIO

TEMPLATE_DIR = 'overlay_templates'
PLATFORM = 'aspeed,ast2500'
OUTPUT_DIR = 'out'
DTOVERLAY = os.path.join('/usr', 'bin', 'dtoverlay')


class TMP75:
    def __init__(self, **kwargs):
        self.name = 'tmp75'
        self.template_args = {'platform': PLATFORM,
                              'bus': kwargs.get('bus'),
                              'reg': kwargs.get('reg')}

    def create_dts(self):
        with open(os.path.join(TEMPLATE_DIR, 'tmp75.template')) as template_file:
            output = Template(template_file.read()).substitute(self.template_args)

        outfile = 'tmp75@{}_{}.dts'.format(self.template_args['bus'], self.template_args['reg'])
        with open(os.path.join(OUTPUT_DIR, outfile), 'w') as file_handle:
            file_handle.write(output)


def create_dtbo():
    for dts in glob.glob(os.path.join(OUTPUT_DIR, '*.dts')):
        dtb = dts.replace('.dts', '.dtbo')
        subprocess.check_call(r'dtc -@ -I dts -O dtb -o {} {}'.format(dtb, dts), shell=True)


def load_overlay():
    for dtb in glob.glob(os.path.join(OUTPUT_DIR, '*.dtbo')):
        basename = os.path.basename(dtb).rstrip('.dtbo')
        subprocess.check_call(r'{} -d {} {}'.format(DTOVERLAY, OUTPUT_DIR, basename), shell=True)


def unload_overlays():
    active = subprocess.check_output(r'{} -d {} -l'.format(DTOVERLAY, OUTPUT_DIR), shell=True)
    buf = StringIO.StringIO(active)
    buf.readline()  # first line isn't important
    for _ in buf:
        subprocess.call(r'{} -d {} -r 0'.format(DTOVERLAY, OUTPUT_DIR), shell=True)


def generate_template(**kwargs):
    type = kwargs.get('type', None)
    if type is None:
        raise KeyError('Missing type argument.')
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
    obj = getattr(sys.modules[__name__], type.upper())(**kwargs)
    obj.create_dts()


if __name__ == '__main__':
    if '-d' in sys.argv:
        unload_overlays()
    elif '-l' in sys.argv:
        create_dtbo()
        load_overlay()
    else:
        if len(sys.argv) < 3:
            raise Exception('Must supply kwargs, -d or -l')
        try:
            keys = [x.split('=')[0] for x in sys.argv[1:]]
            values = [x.split('=')[1] for x in sys.argv[1:]]
        except IndexError:
            raise Exception('Bad input format')

        kwargs = dict(zip(keys, values))
        generate_template(**kwargs)


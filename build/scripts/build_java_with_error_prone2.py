import sys
import os
import re
import subprocess


ERROR_PRONE_FLAGS = [
    '-Xep:FunctionalInterfaceMethodChanged:WARN',
    '-Xep:ReturnValueIgnored:WARN',
]

JAVA10_EXPORTS = [
    '--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.processing=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED',
    '--add-exports=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED',
    '--add-opens=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED'
]


def get_java_version(exe):
    p = subprocess.Popen([exe, '-version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    for line in ((out or '').strip() + (err or '').strip()).split("\n"):
        m = re.match('java version "(.+)"', line)
        if m:
            parts = m.groups()[0].split(".")
            return parts[1] if parts[0] == "1" else parts[0]
        m = re.match('openjdk version "(.+)"', line)
        if m:
            return m.groups()[0]
    return None


def just_do_it(argv):
    java, javac, error_prone_tool, javac_cmd = argv[0], argv[1], argv[2], argv[3:]
    ver = get_java_version(java)
    if not ver:
        raise Exception("Can't determine java version")
    if int(ver) >= 10:
        for f in javac_cmd:
            if f.startswith('-Xep:'):
                ERROR_PRONE_FLAGS.append(f)
        for f in ERROR_PRONE_FLAGS:
            if f in javac_cmd:
                javac_cmd.remove(f)
        os.execv(javac, [javac] + JAVA10_EXPORTS + ['-processorpath', error_prone_tool, '-XDcompilePolicy=byfile'] + [(' '.join(['-Xplugin:ErrorProne'] + ERROR_PRONE_FLAGS))] + javac_cmd)
    else:
        os.execv(java, [java, '-Xbootclasspath/p:' + error_prone_tool, 'com.google.errorprone.ErrorProneCompiler'] + ERROR_PRONE_FLAGS + javac_cmd)


if __name__ == '__main__':
    just_do_it(sys.argv[1:])
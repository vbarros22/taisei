# Taisei's GLSL #include processing (renderer/common/shaderlib/lang_glsl.c),
# closely enough to feed its shaders to gx2gl and CafeGLSL on a PC.
# usage: preprocess.py <shader dir> <file> <FRAG|VERT>
import sys,os,re
root=sys.argv[1]; main=sys.argv[2]; stage=sys.argv[3]
out=[]; st={'ver':False,'hdr':False}
def proc(path,level):
    lines=open(os.path.join(root,path)).read().split('\n')
    if lines and lines[-1]=='': lines=lines[:-1]
    # The main file gets no line marker before its first line (#version).
    need=level>0
    for n,l in enumerate(lines,1):
        s=l.strip()
        if s.startswith('#include'):
            f=re.search(r'"(.*)"',s).group(1)
            inc=os.path.normpath(os.path.join(os.path.dirname(path),f))
            out.append('// begin include (level %d)'%level)
            proc(inc,level+1)
            out.append('// end include (level %d)'%level)
            need=True; continue
        if s.startswith('#version'):
            # Like glsl_write_version() + glsl_write_header(): the stage
            # and backend macros right after #version.
            out.append('#version 330 core')
            out.append('#define %s_STAGE'%stage); out.append('#define BACKEND_GL33 1')
            need=True; continue
        if need:
            out.append('// file: %s'%path); out.append('#line %d %d'%(n,level)); need=False
        out.append(l)
proc(main,0)
print('\n'.join(out))

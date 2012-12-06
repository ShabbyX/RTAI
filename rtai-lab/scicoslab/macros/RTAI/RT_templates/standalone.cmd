[CCode,FCode]=gen_blocks()
[Code,Code_common]=make_standalonert();
files=write_code(Code,CCode,FCode,Code_common);
Makename=rt_gen_make(rdnom,files,archname);
ok=compile_standalone();

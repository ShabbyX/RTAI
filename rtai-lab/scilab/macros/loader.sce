mode(-1);
// specific part
libname='rtai' // name of scilab function library [CUSTOM]
DIR = get_absolute_file_path('loader.sce');

// You should also modify the  ROUTINES/loader.sce with the new 
// Scilab primitive for the path

rtailib = lib(DIR+'/macros/')
exec('SCI/contrib/RTAI/routines/loader.sce');

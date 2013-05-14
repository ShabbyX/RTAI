//exec file used to load the "compiled" block into Scilab
rdnom='pid'
// get the absolute path of this loader file
[units,typs,nams]=file();clear units typs;
for k=size(nams,'*'):-1:1
   l=strindex(nams(k),rdnom+'_loader.sce');
   if l<>[] then
     DIR=part(nams(k),1:l($)-1);
     break
   end
end
archname=emptystr()
Makename = DIR+rdnom+'_Makefile';
select getenv('COMPILER','NO');
case 'VC++'   then
  Makename = strsubst(Makename,'/','\')+'.mak';
case 'ABSOFT' then
  Makename = strsubst(Makename,'/','\')+'.amk';
end
libn=ilib_compile('libpid',Makename)
//unlink if necessary
[a,b]=c_link(rdnom); while a ;ulink(b);[a,b]=c_link(rdnom);end
link(libn,rdnom,'c')
//load the gui function
getf(DIR+'/'+rdnom+'_c.sci');

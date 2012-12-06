function [x,y,typ] = rtai4_extdata(job,arg1,arg2)
  x=[];y=[];typ=[];
  select job
  case 'plot' then
    exprs=arg1.graphics.exprs;
    filename=exprs(1)
    standard_draw(arg1)
  case 'getinputs' then
    [x,y,typ]=standard_inputs(arg1)
  case 'getoutputs' then
    [x,y,typ]=standard_outputs(arg1)
  case 'getorigin' then
    [x,y]=standard_origin(arg1)
  case 'set' then
    x=arg1
    model=arg1.model;graphics=arg1.graphics;
    exprs=graphics.exprs;
    while %t do
      [ok,filename,npts,exprs]=..
      getvalue('Set extdata data block parameters',..
      ['Filename (max 29 chars) :';
       'Nr. of Points:'],..
      list('str',1,'vec',-1),exprs)
      if ~ok then break,end
      if exists('outport') then out=ones(outport,1), in=[], else out=1, in=[], end
      [model,graphics,ok]=check_io(model,graphics,in,out,1,[])
      if ok then
        graphics.exprs=exprs;
        model.rpar=[];
        model.ipar=[npts;0;length(filename);ascii(filename)'];
        model.dstate=[];
        x.graphics=graphics;x.model=model
        break
      end
    end
  case 'define' then
    filename='test.dat'
    npts=1000
    model=scicos_model()
    model.sim=list('rtextdata',4)
    if exists('outport') then model.out=ones(outport,1), model.in=[], else model.out=1, model.in=[], end
    model.evtin=1
    model.rpar=[]
    model.ipar=[npts;0;length(filename);ascii(filename)']
    model.dstate=[];
    model.blocktype='d'
    model.dep_ut=[%t %f]
    exprs=[filename,sci2exp(npts)]
    gr_i=['xstringb(orig(1),orig(2),[''extdata'';filename],sz(1),sz(2),''fill'');']
    x=standard_define([3 2],model,exprs,gr_i)
  end
endfunction

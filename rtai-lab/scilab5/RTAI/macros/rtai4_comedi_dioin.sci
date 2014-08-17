function [x,y,typ] = rtai4_comedi_dioin(job,arg1,arg2)
  x=[];y=[];typ=[];
  select job
  case 'plot' then
    exprs=arg1.graphics.exprs;
    ch=exprs(1)
    name=exprs(2)
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
      [ok,ch,name,exprs]=..
      scicos_getvalue('Set RTAI-COMEDI DIO block parameters',..
      ['Channel:';
       'Device:'],..
      list('vec',-1,'str',1),exprs)
      if ~ok then break,end
      if exists('outport') then out=ones(outport,1), in=[], else out=1, in=[], end
      [model,graphics,ok]=check_io(model,graphics,in,out,1,[])
      dev=str2code(name)
      if ok then
        graphics.exprs=exprs;
        model.rpar=[];
        model.ipar=[ch;
                    dev(length(dev))];
        model.dstate=[];
        x.graphics=graphics;x.model=model
        break
      end
    end
  case 'define' then
    ch=0
    name='comedi0'
    model=scicos_model()
    model.sim=list('rt_comedi_dioin',4)
    if exists('outport') then model.out=ones(outport,1), model.in=[], else model.out=1, model.in=[], end
    model.evtin=1
    model.rpar=[]
    model.ipar=[ch;
                0]
    model.dstate=[];
    model.blocktype='d'
    model.dep_ut=[%t %f]
    exprs=[sci2exp(ch);name]
    gr_i=['xstringb(orig(1),orig(2),[''COMEDI DI'';name+'' CH-''+string(ch)],sz(1),sz(2),''fill'');']
    x=standard_define([3 2],model,exprs,gr_i)
  end
endfunction

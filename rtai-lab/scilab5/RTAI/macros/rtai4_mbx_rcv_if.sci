function [x,y,typ] = rtai4_mbx_rcv_if(job,arg1,arg2)
  x=[];y=[];typ=[];
  select job
  case 'plot' then
    exprs=arg1.graphics.exprs;
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
      [ok,outport,name,ipaddr,exprs]=..
      scicos_getvalue('Set RTAI mbx_rcv_if block parameters',..
      ['Output ports:';
       'MBX Name:';
       'IP Addr:'],..
      list('vec',-1,'str',1,'str',1),exprs)
      if ~ok then break,end
      if exists('outport') then out=ones(outport,1), in=[], else out=1, in=[], end
      [model,graphics,ok]=check_io(model,graphics,in,out,1,[])
      if ok then
        graphics.exprs=exprs;
        model.rpar=[];
        model.ipar=[length(name);
                    length(ipaddr);
                    ascii(name)';
                    ascii(ipaddr)'];
        model.dstate=[];
        x.graphics=graphics;x.model=model
        break
      end
    end
  case 'define' then
    outport=1
    name='MBX1'
    ipaddr='0'
    model=scicos_model()
    model.sim=list('rtai_mbx_rcv_if',4)
    if exists('outport') then model.out=ones(outport,1), model.in=[], else model.out=1, model.in=[], end
    model.evtin=1
    model.rpar=[]
    model.ipar=[length(name);
                length(ipaddr);
                ascii(name)';
                ascii(ipaddr)']
    model.dstate=[];
    model.blocktype='d'
    model.dep_ut=[%t %f]
    exprs=[sci2exp(outport);name;ipaddr]
    gr_i=['xstringb(orig(1),orig(2),[''Mbx Rcv no blk'';name],sz(1),sz(2),''fill'');']
    x=standard_define([3 2],model,exprs,gr_i)
  end
endfunction

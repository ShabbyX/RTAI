function [x,y,typ] = rtai4_mbx_send_if(job,arg1,arg2)
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
      [ok,inport,name,ipaddr,exprs]=..
      getvalue('Set RTAI mbx_send_if block parameters',..
      ['Input ports:';
	'MBX Name:';
	'IP address:'],..
      list('vec',-1,'str',1,'str',1),exprs)
      if ~ok then break,end
      if exists('inport') then in=ones(inport,1), out=[], else in=1, out=[], end
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
    inport=1
    name='MBX1'
    ipaddr='127.0.0.1'
    model=scicos_model()
    model.sim=list('rtai_mbx_send_if',4)
    if exists('inport') then model.in=ones(inport,1), model.out=[], else model.in=1, model.out=[], end
    model.evtin=1
    model.rpar=[]
    model.ipar=[length(name);
		  length(ipaddr);
		  ascii(name)';
		  ascii(ipaddr)']
    model.dstate=[];
    model.blocktype='d'
    model.dep_ut=[%t %f]
    exprs=[sci2exp(inport),name,ipaddr]
    gr_i=['xstringb(orig(1),orig(2),[''Mbx Send if'';name],sz(1),sz(2),''fill'');']
    x=standard_define([3 2],model,exprs,gr_i)
  end
endfunction

function [x,y,typ] = rtai4_log(job,arg1,arg2)

  x=[];y=[];typ=[];
  select job
  case 'plot' then
    exprs=arg1.graphics.exprs;
    name=exprs(1);
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
      [ok,name,exprs]=..
      scicos_getvalue('Set RTAI-log block parameters',..
	 ['LOG name:'],..
      list('str',1),exprs)
     if ~ok then break,end
      in=[model.in]
      intyp=[1]
      in2=[model.in2]
      out=[model.out]
      outtyp=[]
      out2=[model.out2]
      evtin=[1]
      evtout=[]
      [model,graphics,ok]=set_io(model,graphics,list([in,in2],intyp),list([out,out2],outtyp),evtin,evtout,[],[]);
      if ok then
	 graphics.exprs=exprs;
	 model.ipar=[length(name);
		      ascii(name)'
		     ];
	 model.rpar=[];
	  model.dstate=[];
	 x.graphics=graphics;x.model=model
	 break
      end
    end
  case 'define' then
   name='LOG';
   model=scicos_model()
   model.sim=list('rtlog',4)
   model.in=-1
   model.in2=-2
   model.intyp=1
   model.out=[]
   model.evtin=[1]
   model.evtout=[]
   model.ipar=[length(name);
		ascii(name)'
		];
   model.rpar=[];
	model.dstate=[];
	model.blocktype='d';
	model.dep_ut=[%t %f];
    exprs=[name]
    gr_i=['xstringb(orig(1),orig(2),[''Log'';name ],sz(1),sz(2),''fill'');'];
    x=standard_define([3 2],model,exprs,gr_i)
  end
endfunction

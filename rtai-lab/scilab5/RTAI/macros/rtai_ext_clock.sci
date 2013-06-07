function [x,y,typ]=rtai_ext_clock(job,arg1,arg2)
  x=[];y=[];typ=[];
	select job
	case 'plot' then
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
			[ok,sampTime,ans]=..
			   scicos_getvalue('Set Exteral Clock parameter',..
			   ['Expected sample time [s]'],..
			   list('vec',-1),exprs(1))
			if ~ok then break,end
			if sampTime <= 0 then
				message('Sampling time must be positive')
				break
			end
			exprs(1)=ans
			code=exprs(2)
			[ok,code]=clkCode(code)
			if ~ok then break,end
			exprs(2)=code
			[model,graphics,ok]=check_io(model,graphics,[],[],[],1)
			if ok then
				x.model=model
				graphics.exprs=exprs
				x.graphics=graphics
				break
			end
		end
	case 'define' then
		sampTime=0.01
		model=scicos_model()
		model.sim=list('rtextclk',4)
		model.in=[]
		model.out=[]
		model.evtin=[]
		model.evtout=1
		model.dstate=[]
		model.rpar=[]
		model.ipar=[]
		model.blocktype='d'
		model.dep_ut=[%f %f]
		exprs=list(sci2exp(sampTime),[])
		gr_i=['xstringb(orig(1),orig(2),[''EXT'';''CLOCK''],sz(1),sz(2),''fill'');']
		x=standard_define([2 2],model,exprs,gr_i)
	end
endfunction

function [ok,code]=clkCode(code)
	// define standard clkCode
	if code==[] then
		tmp=[
		     '// Do NOT change the name of the function!';
		     'void rtextclk(void) {';
		     '	// Insert code here';
		     '}'
		     ];
	else
  	tmp=code
	end

	[src]=x_dialog(['Define External Clock function';
	                'Do NOT change the name of the function!'],..
	               tmp);

	if src<>[] then
		code=src
	end
endfunction

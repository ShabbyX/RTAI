function RTCodeGen(scs_m,k)
 // Codegeneration of a xcos diagram
 // Calling Sequence
 // RTCodeGen(scs_m,k)
 // Parameters
 // scs_m: can be generated from a xcos-diagram using the compile menu-entry
 // k : number of the superblock.
 // Description
 // Open a diagram with xcos. There must be only one clock and one superblock, containing everything. Generate at first the scs_m file using the compile menu entry.
 //
 // Then go to the scilab teminal. Enter scs_m and determine wich number the super block has. It could be 1 or 2.
 // Compile the Code using RTCodeGen(scs_m,1) or RTCodeGen(scs_m,1)


//** ------------- Preliminary I/O section ___________________________________________________________________________

 if argn(2)<1 then
  global scs_m;
 end;

 if argn(2)<2 then
       k=1;
       for i=1:(size(scs_m.objs)-1)
          if scs_m.objs(i).model.sim(1)=="super" then
               k=i;
          end;
       end

   end;


    //** If the clicked/selected block is really a superblock
    //**         <k>
    if scs_m.objs(k).model.sim(1)=="super" then

        XX = scs_m.objs(k); //** isolate the super block to use

//---------------------------------------------------->       THE REAL CODE GEN IS HERE --------------------------------
        //** the real code generator is here
        [ok, XX, alreadyran, flgcdgen, szclkINTemp, freof] =  do_compile_superblock_rt(XX, scs_m, k, %f);


        //**quick fix for sblock that contains scope
       // gh_curwin = scf(curwin)

    else
      //** the clicked/selected block is NOT a superblock
      message("Generation Code only work for a Super Block ! ")
    end

endfunction


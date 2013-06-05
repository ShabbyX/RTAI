README file for the toolbox RTAI

This toolbox allows to generate code for the Linux RTAI environment

The Toolbox is released under the GPL2 License

After installing the toolbow using the Makefile provided with Linux RTAI,
simply add the following line in your ".scilab" file

exec("SCI/contrib/RTAI/loader.sce");

Code generation
------------------

Do not use the menu entry Tools/Code generation. For RTAI Code generation you have to use Simulation/Compile and then go to the scilab terminal
and type :

RTCodeGen()

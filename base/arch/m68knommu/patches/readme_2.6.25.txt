*******************************************************************************

Real-Time Application Interface (version 3.6.2) for MCF5329

Version 1.0
readme.txt
COPYRIGHT 2008 FREESCALE SEMICONDUCTOR, INC., ALL RIGHTS RESERVED

*******************************************************************************

  FREESCALE SEMICONDUCTOR, INC.
  ALL RIGHTS RESERVED
  (c) Copyright 2008 Freescale Semiconductor, Inc.

*******************************************************************************

  PURPOSE:   Provides hard real-time scheduling functionality under
		uClinux on the MCF5329 processor

  AUTHORS: 	Oleksandr Marchenko	(Group Leader)
		Valentin Pavlyuchenko	(Project Leader)
		Mykola Lysenko 		(Lead Developer)

*******************************************************************************

This document contains information about the product.
The notes presented are the latest available.

The following topics are covered:

1. RTAI for MCF5329 project overview
2. Requirements
3. Installation
4. Support Information
5. Current project status
6. What's New
7. Troubleshooting
8. Last minute notes

*******************************************************************************
1. RTAI for MCF5329 project overview
*******************************************************************************

The Real-Time Application Interface (RTAI) is a set of extensions for the Linux
operating system that adds support for hard real-time scheduling for processes.
It is a real-time platform which allows for high predictability and low 
latency.

This project aims to implement support for RTAI (version 3.6.2) under uClinux 
(kernel version 2.6.25) on the MCF5329 microcontroller. The work will consist 
of adding the architecture support code to RTAI as well as making the 
necessary changes to uClinux to enable RTAI support on the ColdFire 
architecture. 

*******************************************************************************
2. Requirements
*******************************************************************************

MCF5329 evaluation board;
x86 compatible computer;
Linux operating system on the x86 computer.

*******************************************************************************
3. Installation
*******************************************************************************

The product is supplied as a set of patches against the uClinux and RTAI
distributions. To use the product, the following packages need to be
downloaded:

- http://www.uclinux.org/ports/coldfire/uClinux-dist-20080808.tar.bz2
- https://www.rtai.org/RTAI/rtai-3.6.2.tar.bz2
- http://www.uclinux.org/pub/uClinux/m68k-elf-tools/tools-20061214/m68k-uclinux-tools-20061214.sh

Run the m68k-uclinux-tools-20061214.sh script as root to install the cross
compilation tools.

Unzip the RTAI and uClinux archives to the same directory:

$ tar xjf uClinux-dist-20080808.tar.bz2
$ tar xjf rtai-3.6.2.tar.bz2

Copy the patches included in this archive to the current directory.

Apply the patches:

$ cd rtai-3.6.2
$ patch -p1 < ../rtai3.6.2-mcf5329.patch
$ cd ../uClinux-dist
$ patch -p1 < ../uClinux-rtai-mcf5329_2.6.25.patch

Build uClinux:

$ cd uClinux-dist
$ make menuconfig

In the configuration window, set:
	Vendor/Product Selection->Vendor = Freescale
	Vendor/Product Selection->Freescale Products = M5329EVB
	Kernel/Library/Defaults Selection->Kernel Version = linux-2.6.x
	Kernel/Library/Defaults Selection->libc Version = uClibc
	Kernel/Library/Defaults Selection->Customize Kernel Settings = y
	Kernel/Library/Defaults Selection->Customize Application/Library Settings = y

In the kernel configuration window, set:
	Enable loadable module support = y
	Enable loadable module support/Module unloading = y
	Processor type and features->Interrupt Pipeline = y

Then in user configuration options select things that you want to compile 
into romfs. You need to build insmod, rmmod, lsmod tools in order to use
RTAI:
	Busybox/Busybox = y
	Busybox/Busybox/Linux Module Utilites/insmod = y
	Busybox/Busybox/Linux Module Utilites/lsmod = y
	Busybox/Busybox/Linux Module Utilites/rmmod = y
	Busybox/Busybox/Linux Module Utilites/Support version 2.6.x Linux kernels = y
Then exit from menuconfig with saving settings.

You may also want to build POSIX threading support (it must be enabled to 
compile some RTAI user-space testsuite modules - "preempt" and "switches").
To enable POSIX threading support configure uClibc:

$ cd uClibc
$ make menuconfig

In the configuration window, set:
	General Library Settings/POSIX Threading Support = y

$ cd ..

$ make

After this command completes, there will be a file named image.bin in the 
images directory. You should download this file to the board at offset 0x40020000
and execute it with the same starting address in order to run Linux.
	
Build RTAI:

$ cd rtai-3.6.2
$ make ARCH=m68knommu CROSS_COMPILE=m68k-uclinux- menuconfig

In the configuration window, set:
General->Linux Source Tree-><full-path-to-uclinux-dist>/linux-2.6.x

$ make

After this command completes, there will be following RTAI kernel modules 
(testsuite modules aren't shown):
.
 `-- base
     |-- arch
     |   `-- m68knommu
     |       `-- hal
     |           `-- rtai_hal.ko
     |-- ipc
     |   |-- bits
     |   |   `-- rtai_bits.ko
     |   |-- fifos
     |   |   `-- rtai_fifos.ko
     |   |-- mbx
     |   |   `-- rtai_mbx.ko
     |   |-- mq
     |   |   `-- rtai_mq.ko
     |   |-- msg
     |   |   `-- rtai_msg.ko
     |   |-- netrpc
     |   |   `-- rtai_netrpc.ko
     |   |-- sem
     |   |   `-- rtai_sem.ko
     |   `-- tbx
     |       `-- rtai_tbx.ko
     |-- sched
     |   |-- rtai_lxrt.ko
     |   `-- rtai_sched.ko
     |-- tasklets
     |   |-- rtai_signal.ko
     |   `-- rtai_tasklets.ko
     |-- usi
     |   `-- rtai_usi.ko
     `-- wd
         `-- rtai_wd.ko

To insert RTAI modules into the uClinux kernel:
$ cd uClinux-dist/romfs/
$ mkdir modules

Copy all modules to this '<path-to-uclinux>/romfs/modules' directory
and recompile uClinux.
Run uClinux on the board.
When Sash console will appear, type:

$ cd modules
$ insmod rtai_hal.ko
$ insmod rtai_sched.ko
$ insmod rtai_sem.ko
$ insmod rtai_fifos.ko
$ insmod rtai_mbx.ko
$ insmod rtai_tbx.ko
$ insmod rtai_bits.ko
$ insmod rtai_mq.ko
$ insmod rtai_signal.ko

*******************************************************************************
4. Testing RTAI
*******************************************************************************

RTAI Testsuite includes 6 tests:
1) Kernel-space tests:
   a) "latency"
   b) "switches"
   c) "preempt"
2) User-space tests:
   a) "latency"
   b) "switches"
   c) "preempt"

Testsuite has the following files:
.
`-- testsuite
    |-- kern
    |   |-- latency
    |   |   |-- latency_rt.ko
    |   |   |-- display
    |   |   `-- README
    |   |-- preempt
    |   |   |-- preempt_rt.ko
    |   |   |-- display
    |   |   `-- README
    |   `-- switches
    |       |-- switches_rt.ko
    |       `-- README
    `-- user
        |-- latency
        |   |-- latency
        |   |-- display
        |   `-- README
        |-- preempt
        |   |-- preempt
        |   |-- display
        |   `-- README
        `-- switches
            |-- switches
            `-- README

These files (except READMEs) with directory structure should be copied 
to to romfs (for example, to '<path-to-uclinux>/romfs/testsuite').

RTAI FIFOs are used in some tests, so following files must be created in 
the '<path-to-uclinux>/romfs/dev directory:

@rtf0,c,150,0
@rtf1,c,150,1
@rtf2,c,150,2
@rtf3,c,150,3

Appropriate character devices will be created in the romfs when building 
uCLinux.

Then recompile uClinux by typing 
$ make

To launch tests:

Run uClinux on the board.
When Sash console will appear, type:

$ cd modules
$ insmod rtai_hal.ko
$ insmod rtai_sched.ko
$ insmod rtai_sem.ko
$ insmod rtai_fifos.ko
$ insmod rtai_mbx.ko
$ insmod rtai_msg.ko
$ cd ../testsuite

To run the kernel tests:
1) The "Latency" test:

	a) To launch in oneshot mode:

		$ cd kern/latency
		$ insmod latency_rt.ko
		$ ./display

		Press Ctrl+C to stop the test when you have seen results.

		Then don't forget to unload latency module:

		$ rmmod latency_rt

		Here is an example of results (taken according to this README):

			/modules> insmod latency_rt.ko
			/modules> kern/latency/display
			
			## RTAI latency calibration tool ##
			# period = 1000000 (ns)
			# avrgtime = 1 (s)
			# do not use the FPU
			# start the timer
			# timer_mode is oneshot
			
			RTAI Testsuite - KERNEL latency (all data in nanoseconds)
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -29837|     -29837|       2644|      45325|      45325|          0
			RTD|     -29712|     -29837|       2522|      41225|      45325|          0
			RTD|     -29537|     -29837|       2495|      53262|      53262|          0
			RTD|     -29225|     -29837|       2635|      49250|      53262|          0
			RTD|     -30275|     -30275|       2049|      45287|      53262|          0
			RTD|     -29587|     -30275|       2555|      40862|      53262|          0
			RTD|     -29575|     -30275|       2664|      47587|      53262|          0
			RTD|     -29875|     -30275|       5866|      59587|      59587|          0
			RTD|     -28537|     -30275|       3469|      51962|      59587|          0
			RTD|     -28762|     -30275|       3290|      55125|      59587|          0
			RTD|     -29162|     -30275|       3051|      47350|      59587|          0
			RTD|     -29275|     -30275|       2771|      55762|      59587|          0
			RTD|     -28912|     -30275|       2942|      48575|      59587|          0
			RTD|     -29862|     -30275|       2838|      55275|      59587|          0
			RTD|     -30275|     -30275|       2787|      49950|      59587|          0
			RTD|     -29825|     -30275|       2151|      43962|      59587|          0
			RTD|     -29762|     -30275|       3155|      46737|      59587|          0
			RTD|     -29600|     -30275|       3132|      54587|      59587|          0
			RTD|     -28550|     -30275|       3145|      55100|      59587|          0
			RTD|     -29187|     -30275|       2232|      49837|      59587|          0
			RTD|     -29225|     -30275|       2554|      46462|      59587|          0
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -28825|     -30275|       2564|      56725|      59587|          0
			RTD|     -29850|     -30275|       2321|      47762|      59587|          0
			RTD|     -28200|     -30275|       2740|      61212|      61212|          0
			^CRTD|     -28200|     -30275|       2740|      61212|      61212|          0
			/modules> rmmod latency_rt
			
			
			CPU USE SUMMARY
			# 0 -> 30126
			END OF CPU USE SUMMARY


	b) To launch in periodic mode:
		Use the following kernel module parameter when loading 
		latency_rt:

		$ insmod latency_rt.ko timer_mode=1

		Other steps are the same as in oneshot mode.

		Here is an example of results (taken according to this README):

			/modules> insmod latency_rt.ko timer_mode=1
			/modules> kern/latency/display
			
			## RTAI latency calibration tool ##
			# period = 1000000 (ns)
			# avrgtime = 1 (s)
			# do not use the FPU
			# start the timer
			# timer_mode is periodic
			
			RTAI Testsuite - KERNEL latency (all data in nanoseconds)
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -29425|     -29425|        -42|      43737|      43737|          0
			RTD|     -35638|     -35638|         -3|      44775|      44775|          0
			RTD|     -33150|     -35638|         -1|      45987|      45987|          0
			RTD|     -33525|     -35638|         14|      47650|      47650|          0
			RTD|     -39550|     -39550|         -5|      44313|      47650|          0
			RTD|     -28763|     -39550|          6|      39863|      47650|          0
			RTD|     -30588|     -39550|          0|      36238|      47650|          0
			RTD|     -29287|     -39550|        -10|      40750|      47650|          0
			RTD|     -46400|     -46400|          4|      50550|      50550|          0
			RTD|     -28438|     -46400|         -5|      43000|      50550|          0
			RTD|     -28675|     -46400|         -2|      45350|      50550|          0
			RTD|     -34525|     -46400|         -9|      55613|      55613|          0
			RTD|     -29475|     -46400|         27|      36037|      55613|          0
			RTD|     -28750|     -46400|        -13|      31363|      55613|          0
			RTD|     -39762|     -46400|          1|      64987|      64987|          0
			RTD|     -32400|     -46400|         -4|      49363|      64987|          0
			RTD|     -38375|     -46400|         13|      38375|      64987|          0
			RTD|     -35450|     -46400|         -4|      53237|      64987|          0
			RTD|     -33100|     -46400|         -4|      44788|      64987|          0
			RTD|     -30200|     -46400|         -4|      44813|      64987|          0
			RTD|     -35300|     -46400|        -10|      49300|      64987|          0
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -30850|     -46400|         13|      36937|      64987|          0
			RTD|     -28275|     -46400|         -1|      52988|      64987|          0
			RTD|     -35025|     -46400|         -4|      53587|      64987|          0
			RTD|     -28925|     -46400|         14|      63163|      64987|     ^C     0
			/modules> rmmod latency_rt
			
			
			CPU USE SUMMARY
			# 0 -> 35194
			END OF CPU USE SUMMARY


2) The "Preempt" test:

	$ cd kern/preempt
	$ insmod preempt_rt.ko
	$ ./display

        Press Ctrl+C to stop the test when you have seen results.

        Then don't forget to unload preempt_rt module:

        $ rmmod preempt_rt

	Here is an example of results (taken according to this README):

			/modules> insmod preempt_rt.ko
			/modules> kern/preempt/display
			RTAI Testsuite - UP preempt (all data in nanoseconds)
			RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
			RTD|      -28000|         217|       43662|      256725|      389650
			RTD|      -31025|        -783|       43662|      256725|      389650
			RTD|      -31025|        -651|       43687|      256725|      389650
			RTD|      -31025|        -486|       55100|      256725|      389650
			RTD|      -31025|        -687|       55100|      256725|      389650
			RTD|      -31025|          49|       59025|      257862|      389650
			RTD|      -31025|        -808|       59025|      257862|      389650
			RTD|      -31025|        5117|       69412|      257862|      389650
			RTD|      -31025|         497|       69412|      257862|      389650
			RTD|      -31025|        -150|       69412|      257862|      389650
			RTD|      -31025|        -369|       69412|      257862|      389650
			RTD|      -31025|        -163|       69412|      257862|      389650
			RTD|      -31025|         -90|       69412|      257862|      389650
			RTD|      -31025|         269|       69412|      257862|      389650
			RTD|      -31025|        -302|       69412|      257862|      389650
			RTD|      -31025|         525|       69412|      257862|      389650
			RTD|      -31025|        -121|       69412|      257862|      389650
			RTD|      -31025|        -107|       69412|      257862|      389650
			RTD|      -31025|         250|       69412|      257862|      389650
			RTD|      -31025|          45|       69412|      257862|      389650
			RTD|      -31025|         143|       69412|      257862|      389650
			RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
			RTD|      -31025|         741|       69412|      257862|      389650
			RTD|      -31025|        -213|       69412|      257862|      389650
			RTD|      -31025|         -86|       69412|      257862|      389650
			^CRTD|      -31025|         -86|       69412|      257862|      389650
			/modules> rmmod preempt_rt.ko
			
			
			CPU USE SUMMARY
			# 0 -> 38439
			END OF CPU USE SUMMARY


3) The "Switches" test:

        $ cd kern/switches
        $ insmod switches_rt.ko

	Results will be shown.

        $ rmmod switches_rt

	Here is an example of results (taken according to this README):

		/modules> insmod switches_rt.ko
		Wait for it ...
		
		
		FOR 10 TASKS: TIME 772 (ms), SUSP/RES SWITCHES 40000, SWITCH TIME 19316 (ns)
		
		FOR 10 TASKS: TIME 814 (ms), SEM SIG/WAIT SWITCHES 40000, SWITCH TIME 20370 (ns)
		
		FOR 10 TASKS: TIME 993 (ms), RPC/RCV-RET SWITCHES 40000, SWITCH TIME 24849 (ns)
		
		/modules> rmmod switches_rt
		
		CPU USE SUMMARY
		# 0 -> 60011
		END OF CPU USE SUMMARY


To run the user-space tests:
1) The "Latency" test:

	a) To launch in oneshot mode:

        	$ cd user/latency
        	$ ./latency&
        	$ ./display

		Press ENTER to stop the test when you have 
		seen the results.

		Here is an example of results (taken according to this README):

			/modules/user/latency> ./latency&
			[41]
			/modules/user/latency>
			## RTAI latency calibration tool ##
			# period = 1000000 (ns)
			# average time = 1 (s)
			# use the FPU
			# start the timer
			# timer_mode is oneshot
			
			
			/modules/user/latency> ./display
			RTAI Testsuite - USER latency (all data in nanoseconds)
			1999/11/30 00:03:40
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -20762|     -20762|      12879|      61300|      61300|          0
			RTD|     -24175|     -24175|      13286|      63550|      63550|          0
			RTD|     -22375|     -24175|      13237|      62987|      63550|          0
			RTD|     -22750|     -24175|      13320|      58937|      63550|          0
			RTD|     -21162|     -24175|      20240|      94900|      94900|          0
			RTD|     -22412|     -24175|      13585|      68275|      94900|          0
			RTD|     -21312|     -24175|      13689|      77112|      94900|          0
			RTD|     -22412|     -24175|      13686|      76050|      94900|          0
			RTD|     -22262|     -24175|      13801|      61325|      94900|          0
			RTD|     -22125|     -24175|      13592|      87687|      94900|          0
			RTD|     -18062|     -24175|      13594|      85300|      94900|          0
			RTD|     -23700|     -24175|      13748|      65287|      94900|          0
			RTD|     -22175|     -24175|      12799|      81012|      94900|          0
			RTD|     -22137|     -24175|      13874|      84525|      94900|          0
			RTD|     -17600|     -24175|      14084|      74662|      94900|          0
			RTD|     -21900|     -24175|      13563|      89200|      94900|          0
			RTD|     -23400|     -24175|      13191|      90075|      94900|          0
			RTD|     -23337|     -24175|      14161|      63750|      94900|          0
			RTD|     -21187|     -24175|      13715|      70725|      94900|          0
			RTD|     -23012|     -24175|      13711|      88062|      94900|          0
			RTD|     -24275|     -24275|      13323|      94862|      94900|          0
			1999/11/30 00:03:57
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -21187|     -24275|      14123|      89650|      94900|          0
			RTD|     -22975|     -24275|      13746|      89775|      94900|          0
			
			RTD|     -23050|     -24275|      14089|      78925|      94900|          0
			
			>>> S = 98.696, EXECTIME = 0.330243


	b) To launch in periodic mode:

		In RTAI in file <path-t-RTAI>/testsuite/user/latency/latency.c
		change the line 38 from
		
		#define TIMER_MODE  0

		to

		#define TIMER_MODE  1

		and perform all steps mentioned in a)

		Here is an example of results (taken according to this README):

			/modules/user/latency> ./latency&
			[29]
			/modules/user/latency>
			## RTAI latency calibration tool ##
			# period = 1000000 (ns)
			# average time = 1 (s)
			# use the FPU
			# start the timer
			# timer_mode is periodic
			
			
			/modules/user/latency> ./display
			RTAI Testsuite - USER latency (all data in nanoseconds)
			1999/11/30 00:01:11
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -38063|     -38063|        -18|      46288|      46288|          0
			RTD|     -44787|     -44787|          6|      55500|      55500|          0
			RTD|     -47300|     -47300|         -1|      50037|      55500|          0
			RTD|     -59113|     -59113|         -9|      39962|      55500|          0
			RTD|    -107975|    -107975|         11|      65013|      65013|          0
			RTD|     -53462|    -107975|         -2|      38987|      65013|          0
			RTD|     -57638|    -107975|        -10|      49675|      65013|          0
			RTD|     -54825|    -107975|        -14|      61000|      65013|          0
			RTD|     -37762|    -107975|         25|      46125|      65013|          0
			RTD|     -54913|    -107975|         -6|      44887|      65013|          0
			RTD|     -38488|    -107975|         10|      43175|      65013|          0
			RTD|     -54100|    -107975|        -14|      44263|      65013|          0
			RTD|     -62212|    -107975|         10|      58238|      65013|          0
			RTD|     -41550|    -107975|          2|      51213|      65013|          0
			RTD|     -38100|    -107975|         -9|      55800|      65013|          0
			RTD|     -57337|    -107975|         -1|      56075|      65013|          0
			RTD|     -41388|    -107975|        -16|      62613|      65013|          0
			RTD|     -34587|    -107975|         23|      52900|      65013|          0
			RTD|     -47612|    -107975|         -5|      34387|      65013|          0
			RTD|     -47275|    -107975|          6|      48525|      65013|          0
			RTD|     -39938|    -107975|         -5|      52775|      65013|          0
			1999/11/30 00:01:28
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -58738|    -107975|          0|      59450|      65013|          0
			RTD|     -58450|    -107975|          8|      44850|      65013|          0
			
			RTD|     -41225|    -107975|         28|      42288|      84713|          0
			
			>>> S = 98.696, EXECTIME = 0.31106

2) The "Preempt" test:

        $ cd user/preempt
        $ ./preempt&
        $ ./display

        Press Ctrl+C twice to stop the test when you have 
	seen the results.

	Here is an example of results (taken according to this README):

		/modules/user/preempt> ./preempt&
		[56]
		/modules/user/preempt> ./display
		RTAI Testsuite - LXRT preempt (all data in nanoseconds)
		RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
		RTD|      -24437|       14935|      106450|      229550|      259862
		RTD|      -24437|       14063|      106450|      229550|      261837
		RTD|      -24437|       13920|      106450|      229550|      261837
		RTD|      -24637|       13809|      106450|      252050|      261837
		RTD|      -24637|       13861|      106450|      252050|      261837
		RTD|      -24637|       14084|      106450|      252050|      261837
		RTD|      -24700|       13676|      106450|      252050|      271437
		RTD|      -24700|       13884|      106450|      252050|      284012
		RTD|      -24700|       13806|      106450|      252050|      284012
		RTD|      -24700|       13916|      106450|      252050|      284012
		RTD|      -24700|       13846|      106450|      252050|      284012
		RTD|      -24700|       13638|      106450|      252050|      284012
		RTD|      -24700|       13830|      106450|      252050|      284012
		RTD|      -24700|       13939|      106450|      252050|      284012
		RTD|      -24700|       13849|      106450|      252050|      284012
		RTD|      -24700|       13768|      106450|      252050|      284012
		RTD|      -24700|       13898|      106450|      252050|      284012
		RTD|      -24700|       13934|      106450|      252050|      284012
		RTD|      -24700|       13860|      106450|      252050|      284012
		RTD|      -24700|       13804|      106450|      252050|      284012
		RTD|      -24700|       13761|      106450|      252050|      284012
		RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
		RTD|      -24712|       13842|      106450|      252050|      284012
		RTD|      -24712|       13894|      106450|      252050|      284012
		
		RTD|      -24712|       13773|      106450|      252050|      284012
		LXRT releases PID 61 (ID: display).


3) The "Switches" test:

        $ cd user/switches
        $ ./switches

	Results will be shown. Don't mention about warning for 
	rt_grow_and_lock_stack(). It's just our notice for RTAI users.

	Here is an example of results (taken according to this README):

		/modules/user/switches> ./switches
		
		
		Wait for it ...
		RTAI WARNING: rt_grow_and_lock_stack() does nothing for systems without MMU
		
		
		FOR 10 TASKS: TIME 876 (ms), SUSP/RES SWITCHES 20000, SWITCH TIME 43814 (ns)
		
		FOR 10 TASKS: TIME 949 (ms), SEM SIG/WAIT SWITCHES 20000, SWITCH TIME 47476 (ns)
		
		FOR 10 TASKS: TIME 1263 (ms), RPC/RCV-RET SWITCHES 20000, SWITCH TIME 63158 (ns)


*******************************************************************************
5. Support Information
*******************************************************************************

To order any additional information or to resolve arising problems 
contact 

Oleksandr Marchenko (Group Leader),

Laboratory for Embedded Computer Systems (LECS) 
of the Specialized Computer Systems Department 
of the National Technical University of Ukraine "KPI"
and
Innovation Business-Incubator "Polytechcenter" Ltd. - Freescale S3L Partner
37 Peremogy avenue, Kiev-03056, Ukraine
Tel:    +380 44 454-99-00
Email:  Oleksandr.Marchenko@lecs.com.ua 
        re085c@freescale.com


*******************************************************************************
6. Current project status
*******************************************************************************

The port of RTAI itself to MCF5329 is complete.

*******************************************************************************
7. What's New
*******************************************************************************
version 1.0:

Update to RTAI 3.6.2

Fixed rt_set_timer_delay() cache issue that led to freeze of RTAI task for 51 
second.
Miscellaneous fixes in RTDM, RTAI build system, RTAI configuration system.

version 0.5:

Fixed timing constants.
Fixed interrupts enabled checking.
Changed rt_set_timer_delay to use TCN value.
Increased ONESHOT_SPAN value.

Known issues:
1) System hangs for 51 seconds occur occasionally.

version 0.4:

Fixed long delay (up to 51 seconds) when starting periodic timer.

Known issues:
1) RTAI tasks preemption doesn't work properly, so preempt tests hang Linux 
when launched.

version 0.3:

Removed delay when starting timer in periodic mode.
Updated rtai_llimd() to prevent overflows.
Fixed incorrect timer frequency.

Known issues:
1) RTAI tasks preemption doesn't work properly, so preempt tests hang Linux 
when launched.

version 0.2:

A lot of bugs has been solved.
Latency and switches tests are now working.

Known issues:
1) Preempt tests hang Linux when launched.

version 0.1:

This is the first version.

*******************************************************************************
8. Troubleshooting
*******************************************************************************
None

*******************************************************************************
9. Last minute notes
*******************************************************************************
None
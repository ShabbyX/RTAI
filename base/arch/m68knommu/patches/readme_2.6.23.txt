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
(kernel version 2.6.23) on the MCF5329 microcontroller. The work will consist 
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

- http://www.uclinux.org/pub/uClinux/dist/uClinux-dist-test-20080109.tar.bz2
- https://www.rtai.org/RTAI/rtai-3.6.2.tar.bz2
- http://www.uclinux.org/pub/uClinux/m68k-elf-tools/tools-20061214/m68k-uclinux-tools-20061214.sh

Run the m68k-uclinux-tools-20061214.sh script as root to install the cross
compilation tools.

Unzip the RTAI and uClinux archives to the same directory:

$ tar xjf uClinux-dist-test-20080109.tar.bz2
$ tar xjf rtai-3.6.2.tar.bz2

Copy the patches included in this archive to the current directory.

Apply the patches:

$ cd rtai-3.6.2
$ patch -p1 < ../rtai3.6.2-mcf5329.patch
$ cd ../uClinux-dist-test
$ patch -p1 < ../uClinux-rtai-mcf5329_2.6.23.patch

Build uClinux:

$ cd uClinux-dist-test
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
	Busybox = y
	Busybox/Linux Module Utilites/insmod = y
	Busybox/Linux Module Utilites/insmod: lsmod = y
	Busybox/Linux Module Utilites/insmod: modprobe = y
	Busybox/Linux Module Utilites/insmod: rmmod = y
	Busybox/Linux Module Utilites/Support version 2.6.x Linux kernels = y
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
$ cd uClinux-dist-test/romfs/
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
			RTD|     -35125|     -35125|     -10767|      19762|      19762|          0
			RTD|     -35087|     -35125|     -10587|      24400|      24400|          0
			RTD|     -34962|     -35125|     -10519|      20837|      24400|          0
			RTD|     -35062|     -35125|     -10937|      23050|      24400|          0
			RTD|     -34837|     -35125|     -10604|      23350|      24400|          0
			RTD|     -34512|     -35125|     -10523|      21725|      24400|          0
			RTD|     -35087|     -35125|     -10581|      20550|      24400|          0
			RTD|     -34625|     -35125|      -4330|      63125|      63125|          0
			RTD|     -34450|     -35125|      -7149|      26750|      63125|          0
			RTD|     -34925|     -35125|      -9909|      31162|      63125|          0
			RTD|     -35062|     -35125|      -9461|      27562|      63125|          0
			RTD|     -34437|     -35125|     -10197|      26537|      63125|          0
			RTD|     -34100|     -35125|      -9608|      34087|      63125|          0
			RTD|     -34525|     -35125|     -10066|      25187|      63125|          0
			RTD|     -34887|     -35125|      -9570|      30600|      63125|          0
			RTD|     -35125|     -35125|      -9587|      31262|      63125|          0
			RTD|     -33887|     -35125|      -9540|      35825|      63125|          0
			RTD|     -34925|     -35125|      -9896|      33012|      63125|          0
			RTD|     -34250|     -35125|      -9792|      26312|      63125|          0
			RTD|     -35400|     -35400|     -10002|      27700|      63125|          0
			RTD|     -34950|     -35400|      -9703|      41037|      63125|          0
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -34400|     -35400|      -8979|      33762|      63125|          0
			RTD|     -34962|     -35400|      -9574|      26575|      63125|          0
			RTD|     -34437|     -35400|     -10012|      35875|      63125|          0
			RTD|     -34437|     -35400|     -10012|      35875|      63125|          0
			/modules> rmmod latency_rt
			
			
			CPU USE SUMMARY
			# 0 -> 31625
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
			RTD|     -38938|     -38938|        -38|      29388|      29388|          0
			RTD|     -30700|     -38938|         -2|      45225|      45225|          0
			RTD|     -35013|     -38938|          1|      45438|      45438|          0
			RTD|     -33988|     -38938|         16|      32187|      45438|          0
			RTD|     -33737|     -38938|         -2|      29638|      45438|          0
			RTD|     -37412|     -38938|         -6|      36987|      45438|          0
			RTD|     -46225|     -46225|         -8|      42288|      45438|          0
			RTD|     -30463|     -46225|         25|      52575|      52575|          0
			RTD|     -41600|     -46225|        -27|      33037|      52575|          0
			RTD|     -36000|     -46225|          6|      56637|      56637|          0
			RTD|     -37175|     -46225|         -5|      56888|      56888|          0
			RTD|     -39862|     -46225|         13|      51087|      56888|          0
			RTD|     -29637|     -46225|         -8|      37012|      56888|          0
			RTD|     -47725|     -47725|         17|      56600|      56888|          0
			RTD|     -38100|     -47725|          2|      44275|      56888|          0
			RTD|     -31050|     -47725|         -1|      41700|      56888|          0
			RTD|     -42287|     -47725|          5|      41112|      56888|          0
			RTD|     -43863|     -47725|         -1|      51362|      56888|          0
			RTD|     -36362|     -47725|        -14|      46537|      56888|          0
			RTD|     -36175|     -47725|        -13|      55163|      56888|          0
			RTD|     -36688|     -47725|          6|      44438|      56888|          0
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -38000|     -47725|         17|      65550|      65550|          0
			RTD|     -36225|     -47725|          5|      33162|      65550|          0
			RTD|     -35988|     -47725|         -2|      49775|      65550|          0
			RTD|     -35988|     -47725|         -2|      49775|      65550|          0
			/modules> rmmod latency_rt
			
			
			CPU USE SUMMARY
			# 0 -> 30094
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
		RTD|      -34550|       -9766|       26662|      207938|      270075
		RTD|      -34625|       -9866|       26662|      207938|      270075
		RTD|      -34625|       -9956|       26662|      207938|      270075
		RTD|      -34625|       -9814|       26662|      207938|      270075
		RTD|      -34662|       -9954|       27687|      207938|      270075
		RTD|      -34787|       -9933|       29337|      207938|      270075
		RTD|      -34787|       -9739|       29337|      207938|      270075
		RTD|      -34787|      -10041|       29337|      207938|      270075
		RTD|      -34787|       -7482|       48662|      207938|      270075
		RTD|      -34787|       -3947|       48662|      207938|      270075
		RTD|      -34787|       -5646|       48662|      207938|      270075
		RTD|      -34787|       -8746|       48662|      207938|      270075
		RTD|      -34787|       -8988|       48662|      207938|      270075
		RTD|      -34787|       -8650|       51200|      207938|      270075
		RTD|      -34787|       -8890|       51200|      207938|      270075
		RTD|      -34787|       -8800|       51200|      207938|      270075
		RTD|      -34862|       -9065|       51200|      207938|      270075
		RTD|      -34862|       -8662|       51200|      207938|      270075
		RTD|      -34862|       -8918|       51200|      207938|      270075
		RTD|      -34862|       -9161|       51200|      207938|      270075
		RTD|      -34862|       -8703|       51200|      207938|      270075
		RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
		RTD|      -34862|       -7981|       51200|      207938|      270075
		RTD|      -34862|       -8718|       51200|      207938|      270075
		RTD|      -34862|       -8970|       51200|      207938|      270075
		RTD|      -34862|       -8970|       51200|      207938|      270075
		/modules> rmmod preempt_rt
		
		
		CPU USE SUMMARY
		# 0 -> 39618
		END OF CPU USE SUMMARY

3) The "Switches" test:

        $ cd kern/switches
        $ insmod switches_rt.ko

	Results will be shown.

        $ rmmod switches_rt

	Here is an example of results (taken according to this README):

		/modules> insmod switches_rt.ko
		
		Wait for it ...
		
		
		FOR 10 TASKS: TIME 669 (ms), SUSP/RES SWITCHES 40000, SWITCH TIME 16726 (ns)
		
		FOR 10 TASKS: TIME 729 (ms), SEM SIG/WAIT SWITCHES 40000, SWITCH TIME 18233 (ns)
		
		FOR 10 TASKS: TIME 896 (ms), RPC/RCV-RET SWITCHES 40000, SWITCH TIME 22423 (ns)
		
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

			modules/user/latency> ./latency&
			[29]
			/modules/user/latency>
			## RTAI latency calibration tool ##
			# period = 1000000 (ns)
			# average time = 1 (s)
			# use the FPU
			# start the timer
			# timer_mode is oneshot
			
			
			/modules/user/latency> ./display
			RTAI Testsuite - USER latency (all data in nanoseconds)
			1999/11/30 00:00:24
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -27637|     -27637|       5594|      60962|      60962|          0
			RTD|     -28025|     -28025|       7061|      68112|      68112|          0
			RTD|     -28037|     -28037|       6198|      56812|      68112|          0
			RTD|     -28212|     -28212|       6309|      64350|      68112|          0
			RTD|     -28325|     -28325|      12950|      75125|      75125|          0
			RTD|     -28175|     -28325|       7608|      68475|      75125|          0
			RTD|     -28700|     -28700|       6824|      64375|      75125|          0
			RTD|     -27637|     -28700|       6686|      62912|      75125|          0
			RTD|     -27500|     -28700|       6670|      64862|      75125|          0
			RTD|     -27512|     -28700|       7049|      64300|      75125|          0
			RTD|     -27437|     -28700|       7082|      64887|      75125|          0
			RTD|     -28062|     -28700|       6357|      63987|      75125|          0
			RTD|     -27875|     -28700|       6040|      70575|      75125|          0
			RTD|     -28475|     -28700|       6758|      64212|      75125|          0
			RTD|     -28112|     -28700|       7383|      69787|      75125|          0
			RTD|     -28625|     -28700|       6526|      67287|      75125|          0
			RTD|     -27787|     -28700|       7013|      66675|      75125|          0
			RTD|     -27687|     -28700|       6440|      64337|      75125|          0
			RTD|     -27550|     -28700|       6959|      66712|      75125|          0
			RTD|     -27587|     -28700|       6241|      63650|      75125|          0
			RTD|     -27825|     -28700|       7044|      61825|      75125|          0
			1999/11/30 00:00:29
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -27650|     -28700|       7470|      68887|      75125|          0
			RTD|     -28175|     -28700|       6554|      63612|      75125|          0
			RTD|     -27787|     -28700|       7069|      65837|      75125|          0
			
			RTD|     -28162|     -28700|       6756|      68500|      75125|          0
			
			>>> S = 98.696, EXECTIME = 0.288742


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
			1999/11/30 00:00:28
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -46375|     -46375|        -61|      54775|      54775|          0
			RTD|     -38512|     -46375|         14|      57762|      57762|          0
			RTD|     -47088|     -47088|        -32|      51937|      57762|          0
			RTD|     -50838|     -50838|         13|      57025|      57762|          0
			RTD|     -38075|     -50838|         19|      55813|      57762|          0
			RTD|     -89137|     -89137|         -9|      69487|      69487|          0
			RTD|     -42762|     -89137|         -9|      61450|      69487|          0
			RTD|     -57275|     -89137|         11|      56725|      69487|          0
			RTD|     -66550|     -89137|         -5|      58813|      69487|          0
			RTD|     -56713|     -89137|          2|      93825|      93825|          0
			RTD|     -77363|     -89137|          0|      62300|      93825|          0
			RTD|     -44500|     -89137|        -22|      61737|      93825|          0
			RTD|     -32262|     -89137|         19|      45662|      93825|          0
			RTD|     -43762|     -89137|          8|      61087|      93825|          0
			RTD|     -50813|     -89137|        -11|      59000|      93825|          0
			RTD|     -40688|     -89137|          5|      55587|      93825|          0
			RTD|     -45950|     -89137|          0|      59487|      93825|          0
			RTD|     -36712|     -89137|        -12|      62163|      93825|          0
			RTD|     -39787|     -89137|         19|      61900|      93825|          0
			RTD|     -58025|     -89137|         -6|      52488|      93825|          0
			RTD|     -46750|     -89137|        -22|      56063|      93825|          0
			1999/11/30 00:00:33
			RTH|    lat min|    ovl min|    lat avg|    lat max|    ovl max|   overruns
			RTD|     -46275|     -89137|         12|      60263|      93825|          0
			RTD|     -46913|     -89137|         16|      54750|      93825|          0
			
			RTD|     -35350|     -89137|        -14|      47237|      93825|          0
			
			>>> S = 98.696, EXECTIME = 0.259467
2) The "Preempt" test:

        $ cd user/preempt
        $ ./preempt&
        $ ./display

        Press Ctrl+C twice to stop the test when you have 
	seen the results.

	Here is an example of results (taken according to this README):

			/modules/user/preempt> ./preempt&
			[43]
			/modules/user/preempt> ./display
			RTAI Testsuite - LXRT preempt (all data in nanoseconds)
			RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
			RTD|      -28262|        7408|       69100|      197650|      214387
			RTD|      -28500|        8553|       77925|      209137|      224612
			RTD|      -28500|        7349|       77925|      209137|      224612
			RTD|      -28500|        7441|       77925|      209137|      230025
			RTD|      -28500|        7371|       77925|      209137|      230025
			RTD|      -28500|        7312|       77925|      209137|      230025
			RTD|      -28500|        7316|       77925|      209137|      230025
			RTD|      -28587|        7566|       77925|      209137|      230025
			RTD|      -28587|        7389|       77925|      209137|      230237
			RTD|      -28587|        7434|       77925|      209137|      230237
			RTD|      -28587|        7339|       77925|      209137|      230237
			RTD|      -28587|        7267|       77925|      209137|      230237
			RTD|      -28587|        7316|       77925|      209137|      230237
			RTD|      -28587|        7348|       77925|      209137|      230237
			RTD|      -28587|        7209|       77925|      209137|      230237
			RTD|      -28587|        7461|       77925|      209137|      230237
			RTD|      -28587|        7429|       77925|      209137|      230237
			RTD|      -28587|        7540|       77925|      209137|      246150
			RTD|      -28587|        7354|       77925|      209137|      246150
			RTD|      -28587|        7478|       77925|      209137|      246150
			RTD|      -28587|        7566|       77925|      209137|      246150
			RTH|     lat min|     lat avg|     lat max|    jit fast|    jit slow
			RTD|      -28587|        7685|       77925|      212637|      246150
			RTD|      -28587|        7529|       77925|      212637|      246150
			
			RTD|      -28725|        7583|       77925|      212637|      246150
			LXRT releases PID 48 (ID: display).

3) The "Switches" test:

        $ cd user/switches
        $ ./switches

	Results will be shown. Don't mention about warning for 
	rt_grow_and_lock_stack(). It's just our notice for RTAI users.

	Here is an example of results (taken according to this README):

			/modules/user/preempt> ./switches
			
			Wait for it ...
			RTAI WARNING: rt_grow_and_lock_stack() does nothing for systems without MMU
			
			
			FOR 10 TASKS: TIME 766 (ms), SUSP/RES SWITCHES 20000, SWITCH TIME 38317 (ns)
			
			FOR 10 TASKS: TIME 887 (ms), SEM SIG/WAIT SWITCHES 20000, SWITCH TIME 44380 (ns)
			
			FOR 10 TASKS: TIME 1201 (ms), RPC/RCV-RET SWITCHES 20000, SWITCH TIME 60054 (ns)


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
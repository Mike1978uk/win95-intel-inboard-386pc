  SmartWatch Utilities (250-1033) 

This diskette contains the utility programs for the SmartWatch that are 
needed to automatically set the MS-DOS date and time.

The following files are contained on this diskette:

    SMWCLOCK.COM    README.TXT

[SOURCE]

    SMW.ASM         SMWCLOCK.ASM    SMWCLOCK.COM
    SMWCLOCK.MAP    SMWCLOCK.OBJ    SMWCLOCK.LST


In order to obtain a printout of this README.TXT file, insert this
disk in Drive A, make sure your printer is switched on, is on line,
and at the C:\> prompt, type the following command--

   PRINT A:README.TXT

and press <Enter>.


INSTALLATION

1.  Follow the steps outined in the manual sections for installation of 
    the SmartWatch.

2.. Power up the system as usual.

3.  Copy SMWCLOCK.COM from the utility diskette to the boot disk using the 
    MS-DOS copy command.  Assuming the diskette is in drive a:, the correct
    syntax would be:
          copy a:\*.* c:\dos

4.  Enter the correct Date & Time at the MS-DOS prompt.

To calibrate the smartwatch with the MS-DOS date and time:

 Type SMWCLOCK C <ENTER>.

The SmartWatch will be set and the Date & Time will be redisplayed.
This step needs to be done only ONCE at installation time. The date & time
will be updated and retained with or without power to the system.

To load the correct Date & Time to MS-DOS:

 Type SMWCLOCK S <ENTER>. 

The above line can be put in the AUTOEXEC.BAT file if you wish for the SmartWatch time to be set each time the computer boots.

NOTES: The Tandy 1000 SL has an SLCLK.COM file on the MS-DOS Supplemental
       Programs diskette. DO NOT USE the SMWCLOCK file, it will not work. 
       Use the MS-DOS Date and Time commands to set the correct date and
       time.

       Type SLCLK C and press <ENTER> to calibrate the SmartWatch date and
       time. Use this command any time you wish to change the date and time
       maintained by the SmartWatch.

       To set the system date and time from the SmartWatch date and time,
       type SLCLK S and press <ENTER>. You can also set the system date and
       time automatically whenever you start up your computer. To do this,
       include the SLCLK S command in the AUTOEXEC.BAT file.

       To view the current date and time maintained by the SmartWatch, type
       SLCLK F and press <ENTER>. This command displays the SmartWatch date
       and time without updating the system date and time.


NOTE:
"While reasonable efforts have been taken in the preparation of this
information to assure its accuracy, Tandy Corporation/Radio Shack
assumes no liability resulting from the use of the information
contained herein."

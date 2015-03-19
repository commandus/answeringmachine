answeringmachine
================

Simple SIP client playing WAV file on call
------------------------------------------

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/.

This is a beta.

Build
-----

Build pjsip framework. 
Check target architeture & platform e.g. x86_64, unknown-linux-gnu
If target is x86_64, unknown-linux-gnu.a libraries,  
./configure  
make  

If not, replace pj-x86_64-unknown-linux-gnu to target you have in the configure.ac, then  
aclocal;autoreconf --install;automake --add-missing --copy;./configure  
make  


### Building Windows

Open answeringmachine.vc, change include and library path.

Dependencies
------------

Libraries
* [pjsip] (http://www.pjsip.org/)
* [argtable2] (http://argtable.sourceforge.net/) 

Start
-----

Start as usual (foreground) process:

./answeringmachine -vvvvv -s 192.168.1.1 -p 5060 -i 104 -d 192.168.43.113 -w password -f /answeringmachine/apert.wav -r /answeringmachine/rec.wav  

Start as deamon (or Windows service). 

./answeringmachine -d ...  

Create service in Windows:  
sc create "answeringmachine" binPath= <path>\answeringmachine.exe  
sc delete "answeringmachine"  


Test
----

Place call to phone number (e.g. 104 in domain 192.168.43.113), listen wav file.

### Enable debug output

* -vvvvv	debug info
* -vv		more detailed log
* -v		enable logging

In deamon mode, output routes to syslog (*nix) or <binary path>\answeringmachine.log file. 
Please DO NOT use logging in production.

Other options
=============

Usage answeringmachine  
 [-vdh] -s <uri> [-p <port>] -i <name> -d <name> [-w <key>] -f <file> -r <file>  
play wav file on call  
  -s, --sip=<uri>           SIP registrar host name or IP address e.g. 192.168.1.1  
  -p, --port=<port>         SIP registrar port number, default 5060  
  -i, --id=<name>           Identifier  
  -d, --domain=<name>       Domain name. Default is registrar host name or IP address (-h)  
  -w, --password=<key>      password, default empty  
  -f, --play=<file>         WAV file to play  
  -r, --record=<file>       WAV file to record  
  -v, --verbose             severity: -v: error, ... -vvvvv: debug. Default fatal error only.  
  -d, --daemon              start as deamon  
  -h, --help                print this help and exit  


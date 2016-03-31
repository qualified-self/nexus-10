import sys, os, socket
import time, threading
import string
import serial
import struct
import OSC
import json

SETTINGS = None

destinations = [1]   # destination xbees
framecount = 0
verbose = True

oschost = None
oscserv = None
ser     = None

PRESENT = {
    'A' : False,
    'B' : False
}

def cb_msg_ack(msg):
    """ handle ACK flag """
    parts = msg.split()
    if parts[1] == 'A':
        pass


def cb_msg_acc(msg):
    global verbose

    parts = msg.split()
    #print parts

    if parts[1] == 'A':
        osc_send("/gvs/accel/a", [int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6]) ])

    if parts[1] == 'B':
        osc_send("/gvs/accel/b", [int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6]) ])

    if verbose:
        print "ACC {0}".format(msg)

def cb_msg_stt(msg):
    global verbose

    parts = msg.split()

    if parts[1] == 'A':
        osc_send("/gvs/status/a", [int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6]) ])

    if parts[1] == 'B':
        osc_send("/gvs/status/b", [int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6]) ])

    if verbose:
        print "STT {0}".format(msg)

def cb_msg_log(msg):
    global verbose
    if verbose:
        print "LOG {0}".format(msg)

def cb_msg_stim(addr, tags, stuff, source):
    global ser
    print "stimulation message received"
    print stuff
    ser.write("3 {0} {1}\n".format(stuff[0], stuff[1]))
    ser.flush()

msgtype = {
    """
    enum
    {
      kACK           = 1, // acknowledge
      kACC           = 2, // accelerometer
      kCTR           = 3, //3, // control command
      kSTT           = 4, // status command
      kLOG           = 5, // device specific log message

      kSEND_CMDS_END, // marker
    };
    """

    "1" : cb_msg_ack,
    "2" : cb_msg_acc,
#    "3" : cb_msg_ctr,
    "4" : cb_msg_stt,
    "5" : cb_msg_log,
}

def load_config(fname):
    global SETTINGS, verbose
    print "Reading configuration from {0}".format(fname)
    with open(fname) as f:
        SETTINGS = json.loads( f.read() )
        f.close()

    verbose = SETTINGS['verbose']

def osc_send( path, args ):
    global oschost, SETTINGS

    if not SETTINGS['client_persistent_connection']:
        txhost = (SETTINGS['host_ip'], SETTINGS['host_port'])
        oschost = OSC.OSCClient()
        oschost.connect( txhost )

    msg = OSC.OSCMessage()
    msg.setAddress( path )
    #print args
    for a in args:
      msg.append( a )
    
    try:
        oschost.send( msg )
        #if verbose:
        print( "sending message", msg )
    except OSC.OSCClientError:
        print( "error sending message", msg )    

# ##############################################################
import statemachine

class TwosComplement(statemachine.Machine):
    """ Two's Complement state machine defines the current status of installation """
    initial_state = 'idle'

    @statemachine.event
    def transition(self):
        yield 'idle', 'running'
        yield 'running', 'idle'

    @statemachine.after_transition('idle', 'running')
    def announce(self):
        osc_send("/braid/engage")

    @statemachine.after_transition('running', 'idle')
    def announce(self):
        osc_send("/braid/disengage")

# ##############################################################

if __name__ == '__main__':
    print "BRAID2 Body Area Network Message Broker"
    print "(cc) 2014 Luis Rodil-Fernandez"
    print
    load_config("braid2-broker.config")

    print "Listening for incoming nodes on {0} with baudrate {1}".format(SETTINGS['serial'], SETTINGS['baudrate'])
    print "Listening for control messages on port {0}".format(SETTINGS['listen_port'])
    print "Verbose is {0}".format(SETTINGS['verbose'])

    # Open serial port
    try:
        ser = serial.Serial(SETTINGS['serial'], SETTINGS['baudrate'])
    except serial.serialutil.SerialException, e:
        print "(!!!) Failed to open serial port on {0}".format(SETTINGS['serial'])
        sys.exit(1)

    # initialize OSC support
    if SETTINGS['client_persistent_connection']:
        txhost = (SETTINGS['host_ip'], SETTINGS['host_port'])
        oschost = OSC.OSCClient()
        oschost.connect( txhost )

    rxhost = ("127.0.0.1", SETTINGS['listen_port'])
    oscserv = OSC.OSCServer(rxhost)
    oscserv.addMsgHandler("/stimulus", cb_msg_stim)
    oscserv.addDefaultHandlers()

    print "\nStarting OSCServer."
    st = threading.Thread( target = oscserv.serve_forever )
    st.start()

    framecount = 0

    seq = []
    count = 1

    print "(i) Ctrl-C exists."

    Running = True
    while Running:
        try:
            if ser.isOpen():
                for c in ser.read():
                    if c != '\n':
                        seq.append(c)
                    else:
                        cmd = ''.join(str(v) for v in seq)
                        cmd = cmd.strip()
                        if len(cmd) > 3: # ignore degenerate messages
                            cmd = str(cmd)
                            #print cmd
                            # is our command string complete?
                            if ';' == cmd[-1]:
                                # see if we have a handler for this message type
                                thistype = cmd[0]
                                #print "type = {0} ".format(thistype)
                                if thistype in msgtype:
                                    callback = msgtype[thistype]
                                    params = cmd[4:]
                                    #print "args: {0}".format(params)
                                    callback( params )
                                else:
                                    print "message type {0}, not recognized, ignoring...".format(thistype)

                            # clear the read sequence
                            seq = []
                            count += 1
                            break
                time.sleep( 0.005 )
        except KeyboardInterrupt:
            Runing = False
            print "Closing OSCServer."
            oscserv.close()
            print "Waiting for Server-thread to finish."
            st.join()
            print "Closing serial port."
            ser.close()
            print "Done. Goodbye!"
            sys.exit(0)

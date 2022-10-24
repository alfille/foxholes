import sys
import tkinter
from tkinter import simpledialog
from tkinter import filedialog
from tkinter import scrolledtext
from tkinter import ttk
import signal
import subprocess
import queue
import threading
import math
import json
import time

def signal_handler(signal, frame):
    print("\nForced end\n")
    sys.exit(0)

class Param:
    # default parameters
    length = 5
    width = 1
    visits = 1
    poison = 0
    geometry = "grid"
    connection = "rectangular"

    # empty stack
    stack=[]

    def __new__(cls):
        cls.push()

    @classmethod
    def valid(cls):
        # push already called
        if cls._valid():
            # parameters ok, use and remove pushed ones
            cls.shampop()
            return True
        else:
            # parameters not ok, pull back pushed ones
            cls.pop() # known good
            cls._valid() # just to reset calculated properties
            return False
        
    @classmethod
    def _valid(cls):
        # test parameters and calculates some derived properties
        if cls.length < 3 or cls.length > 64:
            return False
            
        if cls.width < 1 or cls.width > 64//3:
            return False

        if cls.poison < 0 or cls.poison > 10:
            return False

        if cls.geometry == "grid":
            cls.total = cls.width * cls.length
        elif cls.geometry == "circle":
            cls.total = cls.width * cls.length
        elif cls.geometry == "triangle":
            cls.width = cls.length
            cls.total = (cls.length * (1+cls.length)) // 2
        else:
            return False

        if cls.total > 64:
            return False

        if cls.visits < 1 or cls.visits > cls.total:
            return False

        if cls.poison > 1:
            cls.program = "fhsolve"
        elif cls.total < 33:
            cls.program = "foxhole32_solver"
        else:
            cls.program = "foxhole64_solver"
        print("Holes {} program {}".format(cls.total,cls.program))

        return cls.connection in ["rectangular","hexgonal","octagonal"]

    @classmethod
    def push(cls):
        # pushes parameters in a stack to test new ones
        cls.stack.extend( [cls.length,cls.width,cls.visits,cls.poison,cls.geometry,cls.connection] )
        
    @classmethod
    def pop(cls):
        # pulls and uses pushed parameters (overwrites test ones)
        if cls.stack.length() > 5:
            cls.connection = cls.stack.pop()
            cls.geometry   = cls.stack.pop()
            cls.poison     = cls.stack.pop()
            cls.visits     = cls.stack.pop()
            cls.width      = cls.stack.pop()
            cls.length     = cls.stack.pop()
        else:
            cls.connection = "rectangular"
            cls.geometry   = "grid"
            cls.poison     = 0
            cls.visits     = 1
            cls.width      = 1
            cls.length     = 5

    @classmethod
    def shampop(cls):
        # pulls and discards pushed parameters (keeps test ones)
        if len(cls.stack) > 5:
            cls.stack.pop()
            cls.stack.pop()
            cls.stack.pop()
            cls.stack.pop()
            cls.stack.pop()
            cls.stack.pop()

    @classmethod
    def filename(cls):
        # Creates a JSON filename frrom settings
        return "FHl{}w{}v{}p{}{}{}.json".format(Param.length,Param.width,Param.visits,Param.poison,Param.geometry[0],4+2*(["rectangular","hexagonal","octagonal"].index(Param.connection)))

    @classmethod
    def commandLine(cls):
        # returns a list for Popen (command and arguments)
        arg = [
        Param.program,
        "-l{}".format(Param.length),
        "-w{}".format(Param.width),
        "-v{}".format(Param.visits),
        "-p{}".format(Param.poison),
        "-{}".format(Param.geometry[0]),
        "-{}".format(4+2*(["rectangular","hexagonal","octagonal"].index(Param.connection))),
        "-j{}".format(Param.filename()),
        "-u"
        ]
        return arg

class Foxhole(tkinter.Frame):
    def __init__(self, master=None):
        super().__init__(master)
        self.master = master


        # make queues for keeping stdout and stderr whilst it is transferred between threads
        self.looper = False
        self.process = None
        self.out_thread = False
        self.outQueue = queue.Queue()
        self.t_run = None

        self.create_menu()
        self.create_widgets()

        Param.valid()
        self.postValid()

    def clear(self):
        self.status.configure(text="Clearing...")
                    
    def Quit(self):
        sys.exit()
        self.master.quit()

    def Length(self):
        l = simpledialog.askinteger("Length","Enter length of foxholes:",initialvalue=Param.length,minvalue=3,maxvalue=64)
        if ( isinstance( l, int ) ):
            Param.push()
            Param.length=l
            if Param.valid():
                self.postValid()
    
    def Width(self):
        l = simpledialog.askinteger("Width","Enter width of foxholes:",initialvalue=Param.width,minvalue=1,maxvalue=64//3)
        if ( isinstance( l, int ) ):
            Param.push()
            Param.width=l
            if Param.valid():
                self.postValid()
    
    def Visits(self):
        l = simpledialog.askinteger("Visits","Number of foxholes visited each day:",initialvalue=Param.visits,minvalue=1,maxvalue=64)
        if ( isinstance( l, int ) ):
            Param.push()
            Param.visits=l
            if Param.valid():
                self.postValid()
    
    def Poison(self):
        l = simpledialog.askinteger("Poison","How many days is a foxhole poisoned?\n (0 for no poison):",initialvalue=Param.poison,minvalue=0,maxvalue=64)
        if isinstance( l, int ):
            Param.push()
            Param.poison=l
            if Param.valid():
                self.postValid()
    
    def Radio(self):
        Param.push()
        Param.geometry = self.tkgeometry.get()
        Param.connection = self.tkconnection.get()
        if not Param.valid():
            self.tkgeometry.set(Param.geometry)
            self.tkconnection.set(Param.connection)
        else:
            self.postValid()

    def makeInt(self,v):
        if isinstance( v, int ):
            return v
        elif isinstance( v, str ):
            return int(v)
        else:
            return None

    def Open(self):
        f = filedialog.askopenfile(title="Select a json file from a Foxhole game")
        if f is not None:
            try:
                j = json.load(f)
                Param.push()
                if 'length' in j:
                    Param.length = self.makeInt(j['length'])
                if 'width' in j:
                    Param.width  = self.makeInt(j['width'])
                if 'visits' in j:
                    Param.visits = self.makeInt(j['visits'])
                if 'poison' in j:
                    Param.poison = self.makeInt(j['poison'])
                if 'geometry' in j:
                    Param.geometry = j['geometry']
                if 'connection' in j:
                    Param.connection = j['connection']
                if Param.valid():
                    self.postValid()
                else:
                    self.status.config(text="Bad values in file")

            except:
                self.status.config(text="JSON parsing error")
        else:
            self.status.config(text="No file")

    def create_widgets(self):
        # overall notebook
        self.win = ttk.Notebook(self.master,width=600,height=500,padding=10)
        self.master.title("Foxhole Solver")
        self.win.pack(side="top",expand=True,fill=tkinter.BOTH)

        # first tab -- show parameters
        settings = tkinter.Frame(self.win)
        self.win.add(settings,text="Settings")
        
        setset = tkinter.LabelFrame( settings,text="Set",bd=4)
        setset.pack(side=tkinter.LEFT,padx=7)
        setsetF = [ttk.LabelFrame(setset,text=x) for x in ["Length","Width","Visits per day","Poisoned days","Geometry","Jump connections"]]
        for x in setsetF:
            x.grid(sticky=tkinter.E+tkinter.W,padx=7)
        self.setL = [ ttk.Label(x,anchor=tkinter.E,pad=4) for x in setsetF ]
        for x in self.setL:
            x.pack(fill=tkinter.X)
        
        setcalc = tkinter.LabelFrame( settings,text="Calculated",bd=4)
        setcalc.pack(side=tkinter.LEFT,padx=7)
        setcalcF = [ttk.LabelFrame(setcalc,text=x) for x in ["Total holes","Game states","Visit patterns","Solver chosen","Default filename"]]
        for x in setcalcF:
            x.grid(sticky=tkinter.E+tkinter.W,padx=7)
        self.calcL = [ ttk.Label(x,anchor=tkinter.E,pad=4) for x in setcalcF ]
        for x in self.calcL:
            x.pack(fill=tkinter.X)

        # second tab -- stdout
        output = tkinter.Frame(self.win)
        self.win.add(output,text="Output")
        self.stdout=scrolledtext.ScrolledText(output,state=tkinter.DISABLED)
        self.stdout.pack(expand=True,fill=tkinter.BOTH)

        # third tab
        JSON = tkinter.Frame(self.win)
        self.win.add(JSON,text="JSON")
        self.JSON=tkinter.scrolledtext.ScrolledText(JSON,state=tkinter.DISABLED)
        self.JSON.pack(expand=True,fill=tkinter.BOTH)
        
        # bottom panel -- exit and status
        self.buttons=tkinter.Frame(self.master,borderwidth=2,relief="flat",background="white")
        tkinter.Button(self.buttons,text="Exit",command=self.Quit).pack(side="left")
        self.status = tkinter.Label(self.buttons,text="Edit mode",relief="sunken",anchor="e")
        self.status.pack(side="left",fill="both",expand=1)
        self.buttons.pack(side="bottom",fill=tkinter.X)
            
    def about(self):
        print("Foxhole Solve by Paul Alfille 2022")

    def Loop(self):
        # loops while solver runs
        self.stdout.config(state=tkinter.NORMAL)
        while not self.outQueue.empty():
            self.stdout.insert(tkinter.END, self.outQueue.get_nowait())
            self.stdout.see(tkinter.END)
        self.stdout.config(state=tkinter.DISABLED)

        self.loop += 1
        if self.process:
            # Loop again
            #print("Loop again")
            self.status.config(text="Loop {} threads {}".format(self.loop,threading.active_count()))
            self.master.after( 500, func=self.Loop )
        else:
            self.master.after( 2000, func=self.Loop )

    def Stop(self):
        while self.process:
            try:
                self.process.kill()
                #print("Stopping")
            except:
                break
        
    def Run(self):
        # start solve process
        try:
            self.process = subprocess.Popen(
                Param.commandLine(),
    #            ["ping", "localhost", "-c6"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text= True,
                )
        except Exception as e:
            print(e)

        while True:
            output = self.process.stdout.readline()
            if output == '' and self.process.poll() is not None:
                break
            if output:
                self.outQueue.put(output)
        self.process = None

    def postValid(self):
        # update settings display
        self.setL[0].config(text=str(Param.length))
        self.setL[1].config(text=str(Param.width))
        self.setL[2].config(text=str(Param.visits))
        self.setL[3].config(text=str(Param.poison))
        self.setL[4].config(text=Param.geometry)
        self.setL[5].config(text=Param.connection)
        
        # update calculated display
        self.calcL[0].config(text=str(Param.total))
        self.calcL[1].config(text=str( 2 ** Param.total ))
        self.calcL[2].config(text=str( math.comb(Param.total,Param.visits) ))
        self.calcL[3].config(text=Param.program)
        self.calcL[4].config(text=Param.filename())
 
        self.loop = 0
        self.status.config(text="New settings accepted")
        #print(Param.commandLine())
        self.Solve()

    def Solve(self):
        # Clear text fields
        self.stdout.config(state=tkinter.NORMAL)
        self.stdout.delete('1.0',tkinter.END)
        self.stdout.config(state=tkinter.DISABLED)

        # Stop existing solver and thread
        if self.process: # stop solver
            self.Stop()
        if self.t_run: # reap thread
            self.t_run.join(1.0)
            if not self.t_run.is_alive():
                self.t_run = None

        # Start solve thread
        self.t_run = self.startThread( self.Run )

        # Main thread (tkinter) loop for display
        if not self.looper:
            self.looper = True
            self.Loop()

    def startThread(self,target=None):
        if target:
            t = threading.Thread(target=target)
            t.daemon = True
            t.start()
            return t
        else:
            return None

    def create_menu(self):
        menu = tkinter.Menu(self.master,tearoff=0)

        filemenu = tkinter.Menu(menu,tearoff=0)
        menu.add_cascade(label="File",menu=filemenu,underline=0)
        filemenu.add_command(label="Open",command=self.Open)
        filemenu.add_command(label="Save")
        filemenu.add_separator()
        filemenu.add_command(label="Test")
        filemenu.add_separator()
        filemenu.add_command(label="Exit",command=self.Quit)

        editmenu = tkinter.Menu(menu,tearoff=0)
        menu.add_cascade(label="Edit",menu=editmenu,underline=0)
        editmenu.add_command(label="Length",command=self.Length,underline=0)
        editmenu.add_command(label="Width",command=self.Width,underline=0)
        editmenu.add_command(label="Visits/day",command=self.Visits,underline=0)
        editmenu.add_command(label="Poison days",command=self.Poison,underline=0)

        self.tkgeometry=tkinter.StringVar()
        self.tkgeometry.set(Param.geometry)
        geomenu = tkinter.Menu(editmenu,tearoff=0)
        editmenu.add_cascade(label="Geometry",menu=geomenu,underline=0)
        geomenu.add_radiobutton(label="Grid",variable=self.tkgeometry,value="grid",command=self.Radio,underline=0)
        geomenu.add_radiobutton(label="Circle",variable=self.tkgeometry,value="circle",command=self.Radio,underline=0)
        geomenu.add_radiobutton(label="Triangle",variable=self.tkgeometry,value="triangle",command=self.Radio,underline=0)

        self.tkconnection=tkinter.StringVar()
        self.tkconnection.set(Param.connection)
        connectmenu = tkinter.Menu(editmenu,tearoff=0)
        editmenu.add_cascade(label="Connections",menu=connectmenu,underline=0)
        connectmenu.add_radiobutton(label="Rectangular (4)",variable=self.tkconnection,value="rectangular",command=self.Radio,underline=0)
        connectmenu.add_radiobutton(label="Hexagonal (6)",variable=self.tkconnection,value="hexagonal",command=self.Radio,underline=0)
        connectmenu.add_radiobutton(label="Octagonal (8)",variable=self.tkconnection,value="octagonal",command=self.Radio,underline=0)

        helpmenu = tkinter.Menu(menu,tearoff=0)
        menu.add_cascade(label="Help",menu=helpmenu)
        helpmenu.add_command(label="About",command=self.about)

        self.master.config(menu=menu)

def main(args):
    # keyboard interrupt
    signal.signal(signal.SIGINT, signal_handler)

    void = Param()
     
    while True:
        Foxhole(master=tkinter.Tk()).mainloop()

if __name__ == "__main__":
    # execute only if run as a script
    sys.exit(main(sys.argv))

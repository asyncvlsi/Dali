from matplotlib import pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np
from random import randint
import sys
import os 

#print("This is the name of the script: ", sys.argv[0])
#print("Number of arguments: ", len(sys.argv))
#print("The arguments are: " , str(sys.argv))

def name_finder(fp, node_file_name, pl_file_name, plot_all_detail):
    for line in fp:
        if (line.find("circuit_path")!=-1):
            if (line.find("/")==-1):
                pos1 = line.find('"')
                pos2 = line.find('"', pos1+1)
                file_name = "./test/"+line[pos1+1:pos2]+"/"+line[pos1+1:pos2]
                node_file_name = file_name + ".nodes"
                pl_file_name = file_name + "_solution.pl"
                #print(node_file_name)
                #print(pl_file_name)
        if ((line.find("plot_all_detail")!=-1) and (line.find("Yes")!=-1)): plot_all_detail = True
        #print(plot_all_detail)
    return node_file_name, pl_file_name, plot_all_detail

# start of program
plot_all_detail = False # cells in dot or not
node_file_name = ""
pl_file_name = ""

if (len(sys.argv)==1): # if not argument, use default plotting script
    if os.path.isfile("./test/plotter_script.scr"):
        fp = open("./test/plotter_script.scr")
        node_file_name, pl_file_name, plot_all_detail = name_finder(fp, node_file_name, pl_file_name, plot_all_detail)
    else:
        print("Default script missing")
        exit()
elif (len(sys.argv)==2): # if one argument, use this file as the plotting script
    if os.path.isfile(sys.argv[1]):
        fp = open(sys.argv[1])
        #print(sys.argv[1])
        node_file_name, pl_file_name, plot_all_detail = name_finder(fp, node_file_name, pl_file_name, plot_all_detail)
    else:
        print(sys.argv[1]+" script missing")
        exit()
else: # more than one argument, error
    print("Too many arguments. Usage: python Schematic_layout_plotter.py <plot_script>")
    exit()

plt.figure() # create a figure
ax = plt.gca() # create the axe

width_terminal = [] # create the width and height list of terminal and node
height_terminal = []
width_node = []
height_node = []

print(node_file_name)
print(pl_file_name)

if os.path.isfile(node_file_name):
    fp = open(node_file_name)
else:
    print("Node file missing")
    exit()
for line in fp: # open node file and read node size
    pos1 = line.find("\to")
    if (pos1==-1):
    	continue
    else:
    	pos1 = line.find("\t", pos1+1)
    	pos2 = line.find("\t", pos1+1)
    	pos3 = line.find("terminal", pos2+1)
    	if (pos3!=-1): 
            width_terminal.append(int(line[pos1:pos2]))
            height_terminal.append(int(line[pos2:pos3]))
    	else:
            if (plot_all_detail==False):
                width_node.append(int(line[pos1:pos2]))
                height_node.append(int(line[pos2:]))
            else:
                width_terminal.append(int(line[pos1:pos2]))
                height_terminal.append(int(line[pos2:]))
fp.close()

lowerx_terminal = [] # create the location list of terminal and node
lowery_terminal = []
centerx_node = []
centery_node = []

if os.path.isfile(pl_file_name):
    fp = open(pl_file_name)
else:
    print("Placement file missing")
    exit()
for line in fp: # open placement file and read terminal and cell location
    pos1 = line.find("o")
    if (pos1==0):
        pos1 = line.find("\t")
        pos2 = line.find("\t", pos1+1)
        pos3 = line.find("\t", pos2+1)
        pos4 = line.find("FIXED", pos3+1)
        if (pos4!=-1):
            lowerx_terminal.append(int(line[pos1:pos2]))
            lowery_terminal.append(int(line[pos2:pos3]))
        else:
            if (plot_all_detail==False):
                centerx_node.append(int(line[pos1:pos2]))
                centery_node.append(int(line[pos2:pos3]))
            else:
                lowerx_terminal.append(int(line[pos1:pos2]))
                lowery_terminal.append(int(line[pos2:pos3]))
fp.close()

leftbound = lowerx_terminal[0]
rightbound = lowerx_terminal[0] + width_terminal[0]
bottombound = lowery_terminal[0]
topbound = lowery_terminal[0] + height_terminal[0]

for i in range(0, len(lowerx_terminal)): # find the boundaries of terminals, nodes excluded
	if (lowerx_terminal[i]<leftbound): leftbound = lowerx_terminal[i]
	if (lowerx_terminal[i]+width_terminal[i]>rightbound): rightbound = lowerx_terminal[i]+width_terminal[i]
	if (lowery_terminal[i]<bottombound): bottombound = lowery_terminal[i]
	if (lowery_terminal[i]+height_terminal[i]>topbound): topbound = lowery_terminal[i]+height_terminal[i]
	ax.add_patch(Rectangle((lowerx_terminal[i], lowery_terminal[i]), width_terminal[i], height_terminal[i], edgecolor="black", alpha=0.6, facecolor="blue"))

centerx = (leftbound+rightbound)/2.0 # make the figure have the same span in x and y direction
centery = (bottombound+topbound)/2.0

if ((topbound - bottombound)>(rightbound - leftbound)):
	span = topbound - bottombound
else:
	span = rightbound - leftbound

plt.xlim([centerx - span/2.0, centerx + span/2.0])
plt.ylim([centery - span/2.0, centery + span/2.0])

ax.set_aspect('equal')

print(len(centerx_node), "standard cells")

for i in range(0, len(centerx_node)): # plot the center of all nodes
    centerx_node[i] += width_node[i]/2.0
    centery_node[i] += height_node[i]/2.0

plt.plot(centerx_node, centery_node, '.', color='black', markersize=0.1)
plt.show() # show the figure
#!/usr/bin/python3

import os

os.chdir("../build")
os.system("cmake ..")
os.system("make hpcc hpwl")
os.chdir("../bin")

path_to_test = "../test/"
lef_file_list = ["out_1K", "out_10K", "OUT_100K"]

for file in lef_file_list:
	command = "./hpcc -grid 2.1 2.1 -v 5 "
	lef_file = " -lef " + path_to_test + file + ".lef"
	def_file = " -def " + path_to_test + file + ".def"
	res_file = " -o res"+ file
	out_file = " > res" + file + ".txt"
	command += lef_file + def_file + res_file + out_file
	os.system(command)
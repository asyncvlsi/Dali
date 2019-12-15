#!/usr/bin/python3

import os

os.chdir("../build")
os.system("cmake ..")
os.system("make hpcc hpwl")
os.chdir("../bin")

lef_file_list = ["benchmark_1K", "benchmark_10K", "benchmark_100K"]

for file in lef_file_list:
	command = "./hpcc -grid 2.1 2.1 -v 5 "
	lef_file = " -lef " + file + ".lef"
	def_file = " -def " + file + ".def"
	res_file = " -o res"+ file
	out_file = " > res" + file + ".txt"
	command += lef_file + def_file + res_file + out_file
	os.system(command)
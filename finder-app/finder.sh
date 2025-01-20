#!/bin/bash
#authour- Nalin Saxena nasa7792

#this file is the implementation for finder.sh as per the requirements specified in coursera
#Resources - https://devhints.io/bash


#check for number of run time cli args . These should be 2
check_num_input_params() {
    NUM_RUN_TIME_ARGS=$#
	#handle error condition if args are =2
    if [ $NUM_RUN_TIME_ARGS != 2 ]; then 
	    echo "You entered invalid number of arguments !"
	    echo "Please follow the following example format finder.sh /tmp/aesd/assignment1 linux"
	    exit 1
    fi
}


# First check if number of arguments are correct 
check_num_input_params "$@"

#checks if filesdir is valid directory
check_files_dir_valid(){
	#extract first argument
	filesdir=$1 
	#if path is valid throw error and exit
	if [ ! -d "$filesdir" ]; then
		echo "The path provided does not exist or is invalid!"
		echo "Please try again"
		exit 1
	fi
}

#main logic to check number of lines where searchstr was found
count_number_of_instances_of_searchstr(){
path_to_search=$1
string_to_search=$2

num_files=$(find "$path_to_search" -type f | wc -l)
search_hits=$(grep -r -o "$string_to_search" $path_to_search | wc -l)
echo "The number of files are $num_files and the number of matching lines are $search_hits" 
}
#check for path validity (send only first argument)
check_files_dir_valid "$1"
# call for instance searching
count_number_of_instances_of_searchstr "$@"
exit 0

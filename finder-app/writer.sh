#!/bin/bash
#authour- Nalin Saxena nasa7792

#this file is the implementation for writer.sh as per the requirements specified in coursera
#Resources - https://devhints.io/bash


#check for number of run time cli args . These should be 2 Same as finder.sh
check_num_input_params() {
    NUM_RUN_TIME_ARGS=$#
    if [ $NUM_RUN_TIME_ARGS != 2 ]; then
	    echo "You entered invalid number of arguments !"
	    echo "Please follow the following format writer.sh writefile  writestr"
        echo "Example usage  writer.sh /tmp/aesd/assignment1/sample.txt Lorum ipsum"
	    exit 1
    fi
}

# First check if number of arguments are correct 
check_num_input_params "$@"

#checks if filesdir is valid directory
check_files_dir_valid(){
    #extract the directory name 
	filesdir=$(dirname "$1") 
	if [ ! -d "$filesdir" ]; then
		echo "The folder does not exist, creating folder...."
        #creates parent folders as well
        mkdir -p $filesdir
	fi
}
#only send first argument to check path
check_files_dir_valid "$1"

write_text_to_file(){

file_path=$1
content_to_write=$2
#tranfer content to file
echo $content_to_write>$file_path

if [ $? -ne 0 ];then 
    echo "The write operation was unsucessful ! "
    exit 1
    else
    echo "Success ! write operation done"
fi
}
#call file writing function 
write_text_to_file "$@"
exit 0
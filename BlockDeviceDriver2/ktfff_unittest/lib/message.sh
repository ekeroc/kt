#!/bin/bash

function ktfff_info()
{
    local info_str=$1
    Col='\e[0m'
    BIBlu='\e[0;96m'
    printf "\n${BIBlu}[INFO] ${Col}$info_str \n"
}

function ktfff_error()
{
    local error_str=$1
    Col='\e[0m'
    BIPur='\e[1;91m'
    printf "\n${BIPur}[ERR] ${Col}$error_str \n"
}

function ktfff_event()
{
    local event_str=$1
    Col='\e[0m'
    BIBlu='\e[0;95m'
    printf "${BIBlu}[EVENT] ${Col}$event_str \n"
}
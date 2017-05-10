#!/bin/bash
#
# WLAN Minitoring Script Version v2.1
# OpenWrt Atheros wireless radio STA status monitor
# Developed by dir2cas <kalin.t.ivanov@gmail.com>
#
# Comments:
# Required packages: bash, iw, kmod- packages&drivers to get your wireless interface running
# Optional packages: 
# install with $ opkg update; opkg install iw bash ....
# To check if it's installed on your OpenWrt box, try $ opkg list-installed | grep "iw", etc.
# It is also recommended to have bash shell installed on your OpenWrt router
#
# Generally, the script should work without modifying the source, but you are free to do it if satisfying your needs.

SCRIPTNAME="wlan-monitor"

################ SCRIPT GLOBAL VARIABLES ################ 
# Defining varialbles (usually no need to modify)
DIR="/tmp/${SCRIPTNAME}"
FILE="${SCRIPTNAME}.log"
ARP_CACHE="/proc/net/arp"
#########################################################

function Usage() {
# Define the help/warning message:
echo -e "\tUsage: ./${SCRIPTNAME}.sh [OPTION 1] [ARG 1]...[OPTION n] [ARG n]\n\n\tList of available options:\n\t-i :\tscan intensity in seconds\n\t-d :\twireless device/interface to monitor on\n\t-t :\t[STA_MAC | STA_IP | STA_Hostname]\n\t-h, --help :\tshow this menu\n\tDefault is to monitor all stations associated\n\tusing wireless device ${WLAN_DEFAULT} and scan intensity ${MON_INTERVAL_DEFAULT} sec"
}

# Defining the Away() function to terminate the script
function Away() {
# Show the message sent to the terminating function, if any
echo -e >&2 "$@\n"
sleep 1

#do some cleaning
rm -f ${DIR}/* > /dev/null

unset INTERFACE
unset DIR
unset FILE
unset CLIENTS
unset ARP_CACHE
unset HOST
unset WLAN_MON_CMD
unset sta_num
unset sta_list
unset sta_count
unset sta[@]
unset sta_ip[@]
unset sta_hostname[@]
unset sta_linenum[@]
unset sta_mac_arp
unset sta_mac
unset flag
sleep 1
exit
}

# Defining the Scan() function to print the current status of the wireless clients to the temp file:
function Scan() {
#clear
local WLAN_MON_CMD="$1"
${WLAN_MON_CMD} > ${DIR}/${FILE}
}

# Defining the GetHosts() function to learn the IPs and Hostnames of the wireless clients' stations (STAs):
function GetHosts() {
local i=1
sta_num=$(grep -c 'Station' ${DIR}/${FILE})

# Check if there are STAs connected to the AP
if [ "${sta_num}" -eq "0" ]; then
    #exiting
    Away "NO Wireless STAs associated"
fi 

# Get list with the associated STAs' MAC addresses
sta_list=$(cat ${DIR}/${FILE} | grep -e "Station" | cut -f 2 -d" ")

if [ -z "${sta_list}" ]; then     # -n the argument is non empty, -z the argument is empty
    #exiting
    Away "NO Wireless STAs associated"
fi

# Check again the real scanned wireless STAs (MAC addresses in the array)
sta_count=$(echo ${sta_list} | wc -w)

# Get each STA its IP and Hostname 
while [ "${i}" -le "${sta_count}" ]
do
    sta[${i}]=$(echo ${sta_list} | awk -v num=$i '{print $num}')
    #sta[${i}]=$(echo ${sta_list} | cut -f${i} -d" "

    sta_ip[${i}]=$(cat ${CLIENTS} | grep "${sta[${i}]}" | awk '{print $3}')
    if [ -z "${sta_ip[${i}]}" ]; then     # -n the argument is non empty, -z the argument is empty    
        sta_ip[${i}]=$(grep -e "${sta[${i}]}" "${ARP_CACHE}" | awk '{print $1}')
    fi

    sta_hostname[${i}]=$(cat ${CLIENTS} | grep "${sta[${i}]}" | awk '{print $4}')
    if [ -z "${sta_hostname[${i}]}" ] || [ "${sta_hostname[${i}]}" = "*" ]; then   # fix * if hostname for the device is not set    
        sta_hostname[${i}]="noname"
    fi
    #identify the line number of the beginning of each entry
    sta_linenum[${i}]=$(grep -n -E "${sta[${i}]}" ${DIR}/${FILE} | cut -f1 -d: )
    let "i += 1"
done  
}

# Defining the Format() function to prepare the final output of the script
function Format() {
local i=1
local divider="----------------------------------------------------------------------"

while [ "${i}" -le "${sta_count}" ]
do
    line=$(grep -e "${sta[${i}]}" ${DIR}/${FILE})
    sed -i '/'"${sta[${i}]}"'/ s/.*/'"${line} => IP:${sta_ip[${i}]} | Host:${sta_hostname[${i}]}"'/g' ${DIR}/${FILE}
    sed -i '/'"${sta[${i}]}"'/ i'"${divider}"'' ${DIR}/${FILE}
    let "i += 1"
done
}

# Defining the STA_select() function to choose the correct STA, specified by the user with an IP or hostname
function STA_select() {
#Initialize the script argument
local ARG="$1"
local i=1

#Get the probable STA MAC address from the arp cache (the STA may have a static IP, not found in the dhcp file)
#sta_mac_arp=$(arp | grep "${ARG}" | awk '{print $4}')   #this method seems broken, if no arp binary
sta_mac_arp=$(grep -e "${ARG}" "${ARP_CACHE}" | awk '{print $4}')

#Loop to find the STA MAC address that corresponds to the given argument
while [ "${i}" -le "${sta_count}" ]
do
    if [ "${ARG}" = "${sta_hostname[${i}]}" ] || [ "${ARG}" = "${sta_ip[${i}]}" ] || [ "${sta_mac_arp}" = "${sta[${i}]}" ] ; then
        #if the corresponding MAC is found(confirmed by dhcp), parse it to a variable that will be returned
        sta_mac=${sta[${i}]}
    fi
    let "i += 1"    
done

if [ -n "${sta_mac}" ]; then   # -n the argument is non empty, -z the argument is empty
    return "1"
else
    #the given IP or hosname may be of a device that is not a wireless STA, in this case, exiting
    Away "The given IP/Hostname <${ARG}> is not a Wireless STA"
fi
}

# Defining the Main() function
function Main() {

#Define what to do when Ctrl-C is pressed - in this case - to terminate the script jumping to a terminating function
trap '{ echo -e "\nControl-C trap caught, exiting"; Away; }' INT #traps Ctrl-C

#Run an initial scan for wireless clients first to get the information needed:
WLAN_MON_CMD="iw dev ${INTERFACE} station dump"
Scan "${WLAN_MON_CMD}"
GetHosts

#Check which mode of the script to be run:
if [ -z "${HOST}" ]; then     # -n the argument is non empty, -z the argument is empty
    #if no argument given, the script will monitor all STAs, default mode
    WLAN_MON_CMD="iw dev ${INTERFACE} station dump"
else 
    STA_select "${HOST}"
    WLAN_MON_CMD="iw dev ${INTERFACE} station get ${sta_mac}"
fi 

# Start actual monitoring
while :
do
Scan "${WLAN_MON_CMD}"
GetHosts
Format
clear
printf "%s STA(s) associated\n" "${sta_num}"
cat ${DIR}/${FILE}
sleep ${MON_INTERVAL}
done
}

#################################################################################################
#MAIN
#echo "Script parameters are: $@ \nNumber of parameters in the Array: ${#ARGUMENTS[@]}"
declare -a ARGUMENTS=("$@")

# Global variables defaults
WLAN_DEFAULT="wlan0"
#Monitor interval in seconds
MON_INTERVAL_DEFAULT="3"

# Initialize script parameters and usage
if [ "$#" -gt "6" ]; then
    #Usage
    Away "Too many arguments provided\n"
fi

# Handling Script Arguments
for (( i=0;i<${#ARGUMENTS[@]};i++ )); do
case ${ARGUMENTS[${i}]} in
    "-i" )
        if [ -z "${ARGUMENTS[$((${i}+1))]}" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-d" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-t" ]; then 
            Away "###WARNING###\n<${ARGUMENTS[${i}]}> monitoring intensity provided without argument"
        fi    
    ;;
    "-d" )
        if [ -z "${ARGUMENTS[$((${i}+1))]}" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-i" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-t" ]; then 
            Away "###WARNING###\n<${ARGUMENTS[${i}]}> wireless device provided without argument"
        fi
    ;;
    "-t" )
        if [ -z "${ARGUMENTS[$((${i}+1))]}" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-i" ] || [ "${ARGUMENTS[$((${i}+1))]}" = "-d" ]; then 
            Away "###WARNING###\n<${ARGUMENTS[${i}]}> target station provided without argument"
        fi
    ;;
    *help|-h )
        Usage; exit 0;
    ;;
    * )
        if [ "${ARGUMENTS[$((${i}-1))]}" = "-d" ]; then INTERFACE="${ARGUMENTS[${i}]}";
        elif [ "${ARGUMENTS[$((${i}-1))]}" = "-t" ]; then HOST="${ARGUMENTS[${i}]}";
        elif [ "${ARGUMENTS[$((${i}-1))]}" = "-i" ]; then MON_INTERVAL="${ARGUMENTS[${i}]}";
        else
            Away "###WARNING###\n<${ARGUMENTS[${i}]}> is invalid script option\nCheck the help option for a list of available options"
        fi
    ;;
esac
done

# Defining unspecified vital global parameters
if [ -z "${HOST}" ]; then echo -e "Force scanning of all associated STAs"; unset ${HOST}; fi
if [ -z "${INTERFACE}" ]; then echo -e "Force usage of default wireless interface ${WLAN_DEFAULT}"; INTERFACE="${WLAN_DEFAULT}"; fi
if [ -z "${MON_INTERVAL}" ]; then echo -e "Force usage of default scan intensity ${MON_INTERVAL_DEFAULT} seconds"; MON_INTERVAL="${MON_INTERVAL_DEFAULT}"; fi 

CLIENTS=$(cat /etc/config/dhcp | grep "leasefile" | sed 's/.*leasefile//' | tr -d " '") >&- 2>&-
if [ -z "${CLIENTS}" ]; then     # -n the argument is non empty, -z the argument is empty
    CLIENTS="/tmp/dhcp.leases"
fi

# Check if the specified interface is a valid wireless device
$(which iw) dev ${INTERFACE} info 2>/dev/null > /dev/null || { Away "\n<${INTERFACE}> is not a valid wireless interface,\nor not supported by mac80211-based driver"; };
sleep 1

# Check for the script directory in /tmp (RAM):
if [ -d "${DIR}" ]; then
    #echo "Directory exists"
    rm -f ${DIR}/* > /dev/null
    cd ${DIR}
    Main
else 
    echo "Directory does not exists"
    echo "Creating directory"
    mkdir -p ${DIR}
    cd ${DIR}
    Main
fi

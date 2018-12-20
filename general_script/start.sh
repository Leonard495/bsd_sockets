#!/bin/bash

TESTS=/home/user/t

. ./config

function create_namespace {
if [ $# -eq 4 ]
then
IFACE_NAME=$1
SPACE_NAME=$2
IP_POSTFIX=$3
SUBNET=$4
echo "1. Creating Namespace s11 ..."
ip netns add $SPACE_NAME

echo "2. Moving opt11 interface to s11 namespace ..."
ip link set dev ${IFACE_NAME}${IP_POSTFIX} netns $SPACE_NAME

echo "3. Setting ip address to opt11 device ..."
ip netns exec $SPACE_NAME ip addr delete 10.$SUBNET.$IP_POSTFIX.1/24 dev ${IFACE_NAME}${IP_POSTFIX}
ip netns exec $SPACE_NAME ip addr add 10.$SUBNET.1.$IP_POSTFIX/24 dev ${IFACE_NAME}${IP_POSTFIX}

echo "4. Setting link up on opt$N interface ..."
ip netns exec $SPACE_NAME ip link set dev ${IFACE_NAME}${IP_POSTFIX} up

echo "5. Setting default route for $SPACE_NAME namespace ..."
ip netns exec $SPACE_NAME ip route add default via 10.$SUBNET.1.$IP_POSTFIX

# See result
echo "6. Show routes ..."
ip netns exec $SPACE_NAME ip route show
else
echo "Invalid function arguments"
fi
}

function delete_namespace {
if [ $# -eq 1 ]
then
N=$1
ip netns delete $N
else
echo "Invalid parameters for function delete_namespace"
fi
}

iface_name=()
iface_nmb=()
namespace_name=()
test_name=()
subnet_name=()

# 0.1 Removing namespaces
for index in ${!interfaces[*]} #Array length
do
    printf "%d: %s %s\n" $index ${iface_name[$index]} ${iface_nmb[$index]}
    printf "Deleting namespace %s\n" ${interfaces[$index]}
    delete_namespace ${interfaces[$index]}
done

for index in ${!interfaces[*]} #Array length
do
# 1. Extract interface name and number
    item=${interfaces[$index]}
    printf "Interface required: %s\n" $item
    echo $item | sed -e 's/[a-z]/ /g' | sed -e 's/^ *//g'
    INTERFACE_NAME=$(echo "$item" | sed -e 's/[0-9]/ /g' | sed -e 's/ *$//g')
    INTERFACE_NMB=$(echo "$item" | sed -e 's/[a-z]/ /g' | sed -e 's/^ *//g')
    printf "INTERFACE_NMB: %s\n" $INTERFACE_NMB
    printf "INTERFACE_NAME: %s\n" $INTERFACE_NAME
    iface_name+=($INTERFACE_NAME)
    iface_nmb+=($INTERFACE_NMB)

    if [ "$INTERFACE_NAME" == "lan" ]
    then
    subnet_name+=(3)
    namespace_name+=(lan_ns$INTERFACE_NMB)
    else
    subnet_name+=(1)
    namespace_name+=(s$INTERFACE_NMB)
    fi

    test_name+=(${INTERFACE_NAME}_test.sh)

# 2. Create namespace
    printf "iface_name: %s, namespace_name: %s, iface_nmb: %s, test_name: %s\n"  ${iface_name[$index]} ${namespace_name[$index]} ${iface_nmb[$index]} ${test_name[$index]}
    create_namespace ${iface_name[$index]} ${namespace_name[$index]} ${iface_nmb[$index]} ${subnet_name[$index]}
done

printf "\n----------\n"

# 3. Start tests in namespaces
for index in ${!iface_name[*]}
do
    let "md = $index % 2"
    let "q = $index / 2"
    echo "index = $index, md = $md\n"
    
    let "offset_y = q * 100"

    if [ $md -eq 0 ]
    then
    printf "Executing test: %s %s %s %s\n" ${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index+1]}     
    else
    printf "Executing test: %s %s %s %s\n" ${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index-1]}
    fi

    if [ $md -eq 0 ]
    then
#    gnome-terminal --geometry=50x10+0+$offset_y -x ${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index+1]} ${subnet_name[$index]} &
	${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index+1]} ${subnet_name[$index]} &
    else
#    gnome-terminal --geometry=50x10+500+$offset_y -x ${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index-1]} ${subnet_name[$index]} &
	${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index-1]} ${subnet_name[$index]} &
    fi

#    ${TESTS}/${test_name[$index]} $TEST_PARAMS_ITERATIONS ${iface_nmb[$index]} ${iface_nmb[$index+1]} ${subnet_name[$index]} &

    process_id+=($!)
done

printf "\n\n"

sleep 5 # Let all processes start

for pid in ${process_id[*]}
do
    printf "Waiting for the end of process_id#%d\n" $pid
    wait ${pid}
done


# 8. Removing namespaces
for index in ${!iface_name[*]} #Array length
do
    printf "%d: %s %s\n" $index ${iface_name[$index]} ${iface_nmb[$index]}
    printf "Deleting namespace %s\n" ${namespace_name[$index]}
#    delete_namespace ${namespace_name[$index]}
done


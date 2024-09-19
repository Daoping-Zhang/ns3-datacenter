source ./config.sh
RES_DUMP=$NS3/examples/PowerTCP/dump_workload
RES_RESULTS=$NS3/examples/PowerTCP/results_workload
configFile=$NS3/examples/PowerTCP/config-burst.txt #config-burst works just fine with workload as well. Flows are created from within evaluation-workload.cc
MIX_DUMP=$NS3/examples/PowerTCP/mix

mkdir $RES_DUMP
mkdir $MIX_DUMP
mkdir $RES_RESULTS
# algs=(0 1 2 3 4 5 6 7 8)
algs=(0 2 4 7 10)

algNames=("dcqcn" "powerInt" "hpcc" "powerDelay" "timely" "dctcp" "patchedTimely" "swift" "rttqcn" "powerqcn" "ufcc")
CCMODE=(1 3 3 3 7 8 11 12 13 14 15)

# at the moment, power int and delay are called from hpcc ACK function separately and hence cc mode is still 3.

#--wien=true --delayWien=false

wien=false
delay=false

cd $NS3



START=0.1
END=10
FLOWEND=9

LOADS=(0.2 0.4 0.6 0.8 0.9 0.95)

REQ_RATE=(1 4 8 16)
REQ_SIZE=(1000000 2000000 4000000 6000000 8000000 1000000)
# REQ_RATE=(0)

N=1

cdf="$NS3/examples/PowerTCP/websearch.txt"

################################################

# Varying Load. No Incast traffic.

################################################
req=0
query=0
for load in ${LOADS[@]};do
	for algorithm in ${algs[@]};do

		if [[ ${algNames[$algorithm]} == "powerInt" || ${algNames[$algorithm]} == "powerDelay" ]];then
			wien=true
		else
			wien=false
		fi

		if [[ ${algNames[$algorithm]} == "powerDelay" ]];then
			delay=true
		else
			delay=false
		fi

		if [[ ${algNames[$algorithm]} == "timely" || ${algNames[$algorithm]} == "dcqcn" ]];then
			window=0
		else
			window=1
		fi

		sleep 5
		# Check how many cores are being used.
		while [[ $(ps aux|grep "powertcp-evaluation-workload-optimized"|wc -l) -gt 80 ]];do
			echo "Waiting for cpu cores.... $N-th experiment "
			sleep 60
		done

		echo "evaluation-${algNames[$algorithm]}-$load-$req-$query.out $N"
		N=$(( $N+1 ))
		RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}-$load-$req-$query.out"
		# echo "time ./waf --run "evaluation-fairness --algorithm=${CCMODE[$algorithm]} --wien=$wien --delayWien=$delay --windowCheck=$window""
		time ./waf --run "powertcp-evaluation-workload --conf=$configFile --algorithm=${CCMODE[$algorithm]} --wien=$wien --delayWien=$delay --windowCheck=$window --queryRequestRate=$req --load=$load --START_TIME=$START --END_TIME=$END --FLOW_LAUNCH_END_TIME=$FLOWEND --incast=5 --cdfFileName=$cdf --request=$query" > $RESULT_FILE  2> $RESULT_FILE &

		# cat $RESULT_FILE | grep 'FCT' | grep 'flowSize' > $RES_RESULTS/result-${algNames[$algorithm]}-$load-$req.fct
		# cat $RESULT_FILE | grep 'switch 0' | grep 'total' > $RES_RESULTS/result-${algNames[$algorithm]}-$load-$req.buf
	done
done

echo "#######################################"
echo "#      FINISHED LOAD EXPERIMENTS      #"
echo "#######################################"

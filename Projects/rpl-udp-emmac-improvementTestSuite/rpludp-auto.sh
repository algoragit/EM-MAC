#!/bin/bash

POSITION="XorYcoords"
MOTE_IDENT="moteIdentifier"
SIMSPERSCENARIO=4
SIMSPERLOAD=10

declare -a RESULTFILES=('raw' 'delay' 'summary' 'duplicates' 'duplicates_raw');
declare -a PERCENTTOPOLOGIES=('25' '50' '75' '100');
declare -a ENGINECONF=('SMRF' 'BMRF' 'BMRF' 'BMRF'
					   'BMRF' 'BMRF' 'BMRF' 'BMRF'
					   'BMRF');
declare -a MODECONF=(  'MIXED' 'BROADCAST' 'MIXED' 'MIXED'
					   'MIXED' 'MIXED' 'MIXED' 'MIXED'
					   'UNICAST');
declare -a THRESHOLDCONF=(  '1' '1' '1' '2'
                            '3' '4' '5' '6'
                            '1');

declare -a BASEFILES=('base_file_1')

#declare -a PERIODVALUES=('480' '240' '120' '60' '30' '15' '8' '4' '2' '1');
#declare -a PERIODVALUES=('120' '60' '30' '15' '8');
declare -a PERIODVALUES=('0');

declare -a remaining_motes=()
declare -a remaining_motes_aux=()
declare -a motes_names=()
declare -a motes_names_aux=()

for topology in "${BASEFILES[@]}"
do
    mkdir $topology
        echo "Generating random topologies"
        # for load in "${PERIODVALUES[@]}"
        # do
        #     randomseed=$[RANDOM % 999999]
        #     cp $topology".csc" "trafficLoad-"$load"_zolertia.csc"
        #     sed -i s/\#randomseed\#/$randomseed/ "trafficLoad-"$load"_zolertia.csc"
        # done

        for ((j=1; j <= $SIMSPERLOAD; j++))
        do
        	for ((i=0; i<${#PERIODVALUES[@]}; ++i)) 
            do
            echo "Loading project-conf"
            #cp udp-sender_template.c udp-sender.c
            #sed -i s/\#load\#/${PERIODVALUES[i]}/ udp-sender.c

            
            
                randomseed=$[RANDOM % 999999]
                cp $topology".csc" "trafficLoad-"${PERIODVALUES[i]}"_zolertia.csc"
                sed -i s/\#randomseed\#/$randomseed/ "trafficLoad-"${PERIODVALUES[i]}"_zolertia.csc"
                simfile="trafficLoad-"${PERIODVALUES[i]}"_zolertia.csc"

                echo "Starting simulation of "$topology" with "${PERIODVALUES[i]}" traffic load."
                echo "Iteration number "$j
                #make TARGET=cooja nogui=Simulation $simfile
                java -mx512m -jar ../../../contikitsch/tools/cooja/dist/cooja.jar -nogui=$simfile -contiki='../../../contikitsch'


                for filename in "${RESULTFILES[@]}"
                do
                    mv $filename"_results.csv" $topology"/emmac_"$filename"_results_"${PERIODVALUES[i]}"_iteration-"$j".csv"
                done

                mv "packets.dat" $topology"/emmac_"${PERIODVALUES[i]}"_iteration-"$j".dat"
                mv "COOJA.testlog" $topology"/testlog_emmac_"${PERIODVALUES[i]}"_iteration-"$j".txt"
            done 
            #rm udp-sender.c
        done

        #for topologie in "${PERIODVALUES[@]}"
        #do
        #    rm "trafficLoad-"$topologie"_zolertia.csc"
        #done
done
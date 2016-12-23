#!/bin/bash

POSITION="XorYcoords"
MOTE_IDENT="moteIdentifier"
SIMSPERSCENARIO=4
SIMSPERLOAD=2

declare -a RESULTFILES=('raw' 'delay' 'summary' 'duplicates' 'duplicates_raw');
declare -a DENSITYVALUES=('32' '40' '48' '56')
#declare -a DENSITYVALUES=('24')

for ((j=1; j <= $SIMSPERLOAD; j++))
do
    mkdir result_files
        #echo "Generating random topologies"
        # for load in "${PERIODVALUES[@]}"
        # do
        #     randomseed=$[RANDOM % 999999]
        #     cp $topology".csc" "trafficLoad-"$load"_zolertia.csc"
        #     sed -i s/\#randomseed\#/$randomseed/ "trafficLoad-"$load"_zolertia.csc"
        # done

            for density in "${DENSITYVALUES[@]}"
            do
                randomseed=$[RANDOM % 999999]
                cp "nodeDensity-"$density".csc" "nodeDensity-"$density"_zolertia.csc"
                sed -i s/\#randomseed\#/$randomseed/ "nodeDensity-"$density"_zolertia.csc"
                sed -i s/\#outs_count_size\#/$density/ "nodeDensity-"$density"_zolertia.csc"
                sed -i s/\#outs_size\#/$density+1/ "nodeDensity-"$density"_zolertia.csc"
                sed -i s/\#total_nodes\#/$density/ "nodeDensity-"$density"_zolertia.csc"
                sed -i s/300000/7200000/ "nodeDensity-"$density"_zolertia.csc"
                simfile="nodeDensity-"$density"_zolertia.csc"

                echo "Starting simulation of nodeDensity-"$density" traffic load. Iteration number "$j
                java -mx512m -jar ../../tools/cooja/dist/cooja.jar -nogui=$simfile -contiki='../..'


                for filename in "${RESULTFILES[@]}"
                do
                    mv $filename"_results.csv" "result_files/emmac_"$filename"_results_"$density"_iteration-"$j".csv"
                done

                mv "packets.dat" "result_files/emmac_"$density"_iteration-"$j".dat"
                mv "COOJA.testlog" "result_files/testlog_emmac_"$density"_iteration-"$j".txt"
            done 
            #rm udp-sender.c
        
        #for topologie in "${PERIODVALUES[@]}"
        #do
        #    rm "trafficLoad-"$topologie"_zolertia.csc"
        #done
done

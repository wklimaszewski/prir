#include <iostream>
#include <stdio.h>
#include <string>
#include "mpi.h"
#include <stddef.h>
#include <stdlib.h>
#include <fstream>

using namespace std;

int size = 5; //number of traffic lights

typedef struct {
    int id;
    int traffic;
    int hour;
    int min;
    int day;
    int month;
    int year;
}Light;

void writeToFile(){
    //writing sample data to the file

    ofstream outfile;
    outfile.open("trafficInfo.txt");

    int a = 0;
    int mini = 0;
    int hour = 12;

    for(int k = 0; k < 5; k++){

        if(hour == 24)
            hour = 0;

        for(int i = 0; i < size * 12 ; i++){

            if(a == size)
                a = 0;


            if(mini == 59)
                mini = 0;

            outfile << a+1 <<"        "<< (rand()%10)*10 << "   "<<hour<<"  " << mini << "  3   " << "  6   " << "  2019" << endl;

            mini++;
            a++;
        }

        hour++;
    }

    outfile.close();
}

int main()
{
    writeToFile();
    MPI_Init(NULL , NULL);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD , &rank);

    int numP = 0;

    MPI_Comm_size( MPI_COMM_WORLD , &numP );

    int totEntriesPerHour = size*12;

    Light lights[totEntriesPerHour] = {0};

    int elementsPerProc = totEntriesPerHour / numP;

    Light res[size] = {0};

    Light finalArr[size] = {0};

    int nItems = 7;

    MPI_Datatype types[nItems];
    MPI_Datatype lightsType;
    MPI_Aint offsets[nItems];
    int blockLengths[nItems];

    types[0] = MPI_INT;offsets[0] = offsetof(Light , id);blockLengths[0] = 1;

    types[1] = MPI_INT;offsets[1] = offsetof(Light , traffic);blockLengths[1] = 1;

    types[2] = MPI_INT;offsets[2] = offsetof(Light , hour);blockLengths[2] = 1;

    types[3] = MPI_INT;offsets[3] = offsetof(Light , min);blockLengths[3] = 1;

    types[4] = MPI_INT;offsets[4] = offsetof(Light , day);blockLengths[4] = 1;

    types[5] = MPI_INT;offsets[5] = offsetof(Light , month);blockLengths[5] = 1;

    types[6] = MPI_INT;offsets[6] = offsetof(Light , year);blockLengths[6] = 1;


    MPI_Type_create_struct(nItems , blockLengths , offsets , types , &lightsType);
    MPI_Type_commit(&lightsType);


    if(rank == 0){

        Light lights[totEntriesPerHour] = {0};

        Light res[size] = {0};

        ifstream theFile("trafficInfo.txt");

        int lId , lTraffic , lHour , lMin , lDay, lMonth , lYear;

        int cc = 0; //to increment the array by 1 every iteration

        while(theFile >>lId >> lTraffic >> lHour >> lMin >> lDay >> lMin >> lYear)
        {
            lights[cc].id = lId;
            lights[cc].traffic = lTraffic;
            lights[cc].hour = lHour;
            lights[cc].min = lMin;
            lights[cc].day = lDay;
            lights[cc].month = lMonth;
            lights[cc].year = lYear;

            cc++;
        }

        MPI_Scatter(&lights , elementsPerProc , lightsType , MPI_IN_PLACE ,elementsPerProc, lightsType , 0 , MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

        for(int i = 0 ; i < elementsPerProc; i++)
        {
            int tempid = lights[i].id;
            int tempTraffic = lights[i].traffic;

            switch(tempid)
            {
                case 1:{
                    res[0].id = tempid;
                    res[0].traffic += tempTraffic;
                }break;
                case 2:{
                    res[1].id = tempid;
                    res[1].traffic += tempTraffic;
                }break;
                case 3:{
                    res[2].id = tempid;
                    res[2].traffic += tempTraffic;
                }break;
                case 4:{
                    res[3].id = tempid;
                    res[3].traffic += tempTraffic;
                }break;
                case 5:{
                    res[4].id = tempid;
                    res[4].traffic += tempTraffic;
                }break;
            }
        }

        MPI_Gather(MPI_IN_PLACE, size, lightsType , &res , size, lightsType , 0 , MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

    }
    else
    {
        Light lights[elementsPerProc] = {0};

        Light res[size] = {0};

        MPI_Scatter(NULL , elementsPerProc , lightsType , &lights ,elementsPerProc, lightsType , 0 , MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

        for(int i = 0 ; i < elementsPerProc; i++)
        {
            int tempid = lights[i].id;
            int tempTraffic = lights[i].traffic;

            switch(tempid)
            {
                case 1:{
                    res[0].id = tempid;
                    res[0].traffic += tempTraffic;
                }break;
                case 2:{
                    res[1].id = tempid;
                    res[1].traffic += tempTraffic;
                }break;
                case 3:{
                    res[2].id = tempid;
                    res[2].traffic += tempTraffic;
                }break;
                case 4:{
                    res[3].id = tempid;
                    res[3].traffic += tempTraffic;
                }break;
                case 5:{
                    res[4].id = tempid;
                    res[4].traffic += tempTraffic;
                }break;
            }
        }
        MPI_Gather(res, size, lightsType , &res , size, lightsType , 0 , MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

    }

    MPI_Finalize();

    return 0;
}

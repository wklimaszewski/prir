#include <mpi.h>
#define MPE_GRAPHICS
#include <mpe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define WIDTH 1024
#define HEIGHT 768
#define BUFSIZE 25
#define DEBUG 0

int procc, rank;
static MPE_XGraph graph;
static MPE_Color color[256];
static MPE_Point line[WIDTH];

typedef struct {
    double r, i;
} complex;

typedef struct {
    int rowNumber;
    int view;
} rowInfo;

enum {
    tag_free, tag_dim, tag_rows
	};


int mpiInit(int argc, char **argv) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fprintf(stderr, "Failure in initializing MPI\n");
        return 100;
    }

    if (MPI_Comm_size(MPI_COMM_WORLD, &procc) != MPI_SUCCESS) {
        fprintf(stderr, "Couldn't get the process count\n");
        MPI_Finalize();
        return 101;
    } else if (procc < 2) {
        fprintf(stderr, "Must have at least 2 processes (have %d)\n", procc);
        MPI_Finalize();
        return 102;
    }

    if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS) {
        fprintf(stderr, "Couldn't get rank for a process\n");
        MPI_Finalize();
        return 103;
    }

    MPE_Open_graphics(&graph, MPI_COMM_WORLD, NULL, 0, 0, WIDTH, HEIGHT, 0);

    MPE_Make_color_array(graph, 256, color);

    if (DEBUG && !rank)
        printf("MPI Initialized\n");

    return 0;
}

int calc_mandel(complex c) {
    int count = 0;
    int max = 255;
    complex z;
    double len2, temp;
    z.r = z.i = 0.0;
    do {
        temp = z.r*z.r - z.i*z.i + c.r;
        z.i = 2.0*z.r*z.i + c.i;
        z.r = temp;
        len2 = z.r*z.r + z.i*z.i;
        if (len2 > 4.0) break;
        count++;
    } while (count < max);
    return count;
}



void processRow(int row, double *dim) {
    int i;
    double rowPosX;
    double rowPosY;
    double advanceX;
    complex c;

    rowPosY = dim[1] + dim[3]*((double)row/(double)HEIGHT);
    rowPosX = dim[0];
    advanceX = dim[2]/(double)WIDTH;
    c.i = rowPosY;
    c.r = rowPosX;

    for (i = 0; i < WIDTH; ++i) {
        line[i].y = row;
        line[i].x = i;
        c.r += advanceX;
        line[i].c = color[calc_mandel(c)];
    }
    MPE_Draw_points(graph, line, WIDTH);
    MPE_Update(graph);
}


void clientExec() {
    rowInfo rowBuffer[BUFSIZE];
    unsigned char free = BUFSIZE;
    unsigned char tempRowData[WIDTH];

    int i;
    int quit = 0;
    int procIndex = 0, storeIndex = 0;
    int viewCount = 0;
    double dim[4];

    MPI_Status tempStatus;
    MPI_Request *req;
    int reportedRows = BUFSIZE, reportRows;
    int freeRows = 0;
    int nothingToProcess = 0;
    double dimMsg[4];
    int rowMsg[2+BUFSIZE];
    int msgId, commFlag;
    int responsePending;

    for (i = 0; i < BUFSIZE; ++i)
        rowBuffer[i].view = -1;
    req = (MPI_Request*) malloc(sizeof(MPI_Request) * 3);

    reportRows = reportedRows;
    MPI_Isend(&reportRows, 1, MPI_INT, 0, tag_free, MPI_COMM_WORLD, &req[2]);
    responsePending = 1;

    MPI_Irecv(dimMsg, 4, MPI_DOUBLE, 0, tag_dim, MPI_COMM_WORLD, &req[1]);
    MPI_Irecv(rowMsg, 2+BUFSIZE, MPI_INT, 0, tag_rows, MPI_COMM_WORLD, &req[0]);

    if (DEBUG)
        printf("Client %d: Initialized (buffer size %d), switching to main loop\n", rank, BUFSIZE);

    while (!quit) {
        if (DEBUG)
            printf("Client %d: Begin loop.  %d processable rows\n\tBUFSIZE %d, free rows %d, rows in request %d, procIndex %d, storeIndex %d\n",
                    rank, BUFSIZE-freeRows-reportedRows, BUFSIZE, freeRows, reportedRows, procIndex, storeIndex);

        if (reportedRows + freeRows == BUFSIZE
            || nothingToProcess) {
            if (DEBUG)
                printf("Client %d: Empty or only future rows.  Waiting for any message.\n", rank);

            MPI_Waitany(2, req, &msgId, &tempStatus);
        } else {
            MPI_Testany(2, req, &msgId, &commFlag, &tempStatus);
            if (!commFlag)
                msgId = -1;
        }

        switch (msgId) {
            case -1:
                if (DEBUG)
                    printf("Child %d: No messages\n", rank);
                break;

            case 0:

                if (!rowMsg[0]) {
                    if (DEBUG)
                        printf("Child %d: We got quit signal, aborting\n", rank);
                    quit = 1;
                    break;
                }

                responsePending = 0;

                if (DEBUG)
                    printf("Child %d: We got %d rows for view %d (reported buffer spots after this: %d)\n",
                            rank, rowMsg[1], rowMsg[0], reportedRows-rowMsg[1]);

                reportedRows -= rowMsg[1];
                while (rowMsg[1]) {
                    for (; rowBuffer[storeIndex].view >= viewCount;
                            storeIndex = (storeIndex+1)%BUFSIZE);
                    if (rowBuffer[storeIndex].view != -1) {
                        freeRows++;
                        rowBuffer[storeIndex].view = -1;

                        if (DEBUG)
                            printf("Child %d: Spot %d was obsolete (%d < %d)\n",
                                    rank, storeIndex, rowBuffer[storeIndex].view, viewCount);
                    }

                    if (DEBUG)
                        printf("Child %d: Storing request for row %d (view %d) to buffer spot %d.\n",
                                rank, rowMsg[1+rowMsg[1]], rowMsg[0], storeIndex);

                    rowBuffer[storeIndex].view = rowMsg[0];
                    rowBuffer[storeIndex].rowNumber = rowMsg[1+rowMsg[1]];

                    rowMsg[1]--;
                }

                if (DEBUG)
                    printf("Child %d: New row processing ended, spawning a new recv\n", rank);
                MPI_Irecv(rowMsg, 2+BUFSIZE, MPI_INT, 0, tag_rows, MPI_COMM_WORLD, &req[0]);
                break;
            case 1:
                viewCount++;
                memcpy((void*)dim, (void*)dimMsg, sizeof(double)*4);
                if (DEBUG)
                    printf("Child %d: Received new dimensions (x %.3f y %.3f w %.3f h %.3f), increasing viewCount to %d\n",
                           rank, dim[0], dim[1], dim[2], dim[3], viewCount);

                MPI_Irecv(dimMsg, 4, MPI_DOUBLE, 0, tag_dim, MPI_COMM_WORLD, &req[1]);
                break;
        }
        if (quit)
            break;
        if (freeRows + reportedRows == BUFSIZE)
            continue;
        for (i = 0; i < BUFSIZE; ++i, procIndex = (procIndex+1)%BUFSIZE) {
            if (rowBuffer[procIndex].view == -1);
            else if (rowBuffer[procIndex].view < viewCount) {
                if (DEBUG)
                    printf("Child %d: Slot %d is obsolete (%d < %d), freeing in proc loop\n",
                            rank, procIndex, rowBuffer[procIndex].view, viewCount);

                rowBuffer[procIndex].view = -1;
                freeRows++;

            } else if (rowBuffer[procIndex].view == viewCount) {
                if (DEBUG)
                    printf("Child %d: Processing slot %d (view %d, row %d)\n",
                            rank, procIndex, rowBuffer[procIndex].view, rowBuffer[procIndex].rowNumber);

                processRow(rowBuffer[procIndex].rowNumber, dim);
                rowBuffer[procIndex].view = -1;
                freeRows++;
                nothingToProcess = 0;
                break;
            }
        }
        if (i == BUFSIZE)
            nothingToProcess = 1;
        if (freeRows > 0 && !responsePending) {
            MPI_Test(&req[2], &commFlag, &tempStatus);
            if (commFlag) {
                if (DEBUG)
                    printf("Child %d: Previous send of %d slots went through, sending a new one with %d\n",
                            rank, reportRows, freeRows);
                reportedRows += freeRows;
                reportRows = freeRows;
                freeRows = 0;
                MPI_Isend(&reportRows, 1, MPI_INT, 0, tag_free, MPI_COMM_WORLD, &req[2]);
                responsePending = 1;
            }
        }
    }
    if (DEBUG)
        printf("Child %d: Telling the master we're done\n", rank);

    reportRows = 0;
    MPI_Ssend(&reportRows, 1, MPI_INT, 0, tag_free, MPI_COMM_WORLD);

    if (DEBUG)
        printf("Child %d: Terminating execution\n", rank);

void terminate(MPI_Request *req, int *freeSlots) {
    int i;
    int msg;
    MPI_Status tempStatus;


    for (i = 1; i < procc; ++i) {
        if (DEBUG)
            printf("Master: Sending client %d the termination signal\n", i);

        msg = 0;
        MPI_Ssend(&msg, 1, MPI_INT, i, tag_rows, MPI_COMM_WORLD);

        if (DEBUG)
            printf("Master: Waiting for acknowledgement...\n");

        do {
            MPI_Wait(&req[i-1], &tempStatus);
            MPI_Irecv(&freeSlots[i-1], 1, MPI_INT, i, tag_free, MPI_COMM_WORLD, &req[i-1]);
            if (DEBUG)
                printf("Master: Received %d free slots from client %d, expected 0\n",
                        freeSlots[i-1], i);
        } while (freeSlots[i-1]);
        if (DEBUG)
            printf("Master: Client %d has terminated\n", i);
    }
    printf("All clients have terminated.  Master terminating.\n");
}

int getDrag(double *dim) {
    int dragX1, dragX2,
        dragY1, dragY2;
    int dragArea;
    static int dragAreaThreshold = 10;

    MPE_Get_drag_region(graph, 1, 1, &dragX1, &dragY1, &dragX2, &dragY2);
    if (DEBUG)
        printf("Dragged %d %d %d %d\n", dragX1, dragY1, dragX2, dragY2);

    dragArea = (dragX2-dragX1)*(dragY2-dragY1);
    dim[0] += dim[2]*((double)dragX1/(double)WIDTH);
    dim[1] += dim[3]*((double)dragY1/(double)HEIGHT);
    dim[2] *= ((double)(dragX2-dragX1)/(double)WIDTH);
    dim[3] *= ((double)(dragY2-dragY1)/(double)HEIGHT);

    if (DEBUG)
        printf("Drag area %d, threshold %d\n", dragArea, dragAreaThreshold);

    if (dragArea < dragAreaThreshold)
        return 1;
    else
        return 0;
}

void serverExec() {

    double dim[] = {
        -2.0, -2.0, 4.0, 4.0
    };

    int quit = 0;
    int i, j;
    int rowsLeft;
    int viewCount = 0;

    double startTime, tempTime;

    int clients = procc - 1;
    MPI_Request req_rows[clients];
    MPI_Request req_dim[clients];
    MPI_Status sta[clients];
    int completeCount;
    int completed[clients*2];
    int freeSlots[clients];
    int newFreeSlots[clients];
    int tempRowCount;
    int tempRowData[2+HEIGHT];
    for (i = 0; i < clients; ++i) {
        freeSlots[i] = 0;
        MPI_Irecv(&newFreeSlots[i], 1, MPI_INT, i+1, tag_free, MPI_COMM_WORLD, &req_rows[i]);
    }

    if (DEBUG)
        printf("Master: Listeners for %d clients forked\n", clients);

    do {
        startTime = MPI_Wtime();
        viewCount++;
        if (DEBUG)
            printf("Master: Announcing dimensions (%.3f, %.3f, %.3f, %.3f)\n",
                    dim[0], dim[1], dim[2], dim[3]);

        for (i = 0; i < clients; ++i)
            MPI_Isend(dim, 4, MPI_DOUBLE, i+1, tag_dim, MPI_COMM_WORLD, &req_dim[i]);
        rowsLeft = HEIGHT;
        while (rowsLeft) {
            if (MPI_Waitsome(clients, req_rows, &completeCount, completed, sta) != MPI_SUCCESS)
                fprintf(stderr, "Master: Error in reading children status\n");

            if (DEBUG)
                printf("Master: Got %d announcements\n", completeCount);
            for (i = 0; i < completeCount; ++i) {
                if (DEBUG)
                    printf("Master: Client %d reported %d free slots\n",
                            completed[i], newFreeSlots[completed[i]]);
                freeSlots[completed[i]] += newFreeSlots[completed[i]];
                MPI_Irecv(&newFreeSlots[completed[i]], 1, MPI_INT,
                        completed[i]+1, tag_free, MPI_COMM_WORLD, &req_rows[completed[i]]);
                tempRowCount = (HEIGHT/clients < freeSlots[completed[i]]) ?
                    HEIGHT/clients : freeSlots[completed[i]];
                if (tempRowCount > rowsLeft)
                    tempRowCount = rowsLeft;
                tempRowData[0] = viewCount;
                tempRowData[1] = tempRowCount;
                for (j = 0; j < tempRowCount; ++j)
                    tempRowData[2+j] = --rowsLeft;

                if (DEBUG)
                    printf("Master: Sending %d new rows to client %d, leaving %d free\n",
                            tempRowCount, completed[i]+1, freeSlots[completed[i]]-tempRowCount);
                MPI_Ssend(tempRowData, tempRowCount+2, MPI_INT, completed[i]+1, tag_rows, MPI_COMM_WORLD);


                freeSlots[completed[i]] -= tempRowCount;
            }
        }
        tempTime = MPI_Wtime();
        printf("Master: View %d took %.3f seconds\n",
                viewCount, tempTime - startTime);

        if (getDrag(dim))
            quit = 1;
    } while (!quit);

    if (DEBUG)
        printf("Master: Exited the main loop\n");

    terminate(req_rows, newFreeSlots);
}


int main(int argc, char **argv) {
    if (mpiInit(argc, argv)) {
        fprintf(stderr, "Couldn't init MPI\n");
        return 10;
    }

    if (!rank && procc-1 > BUFSIZE)
        printf("WARNING: You have %d clients and %d BUFSIZE.\nTo keep all clients busy, BUFSIZE is recommended to be larger than client count.\n", procc-1, BUFSIZE);

    if (rank)
        clientExec();
    else
        serverExec();

    MPI_Finalize();
    return 0;
}

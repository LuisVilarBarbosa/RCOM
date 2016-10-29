typedef struct {
	int sentFrames;
	int receivedFrames;

	int sentPackets;
	int receivedPackets;

	int sentBytes;
	int receivedBytes;
	unsigned long fileSize;

	int timeouts;

	int sentRR;
	int receivedRR;

	int sentREJ;
	int receivedREJ;
} Statistics;

void printStatistics(Statistics stats) {
	printf("\n");
	printf("=======================\n");
	printf("=CONNECTION STATISTICS=\n");
	printf("=======================\n");

	printf("Sent frames:      %d\n", stats.sentFrames);
	printf("Received frames:  %d\n", stats.receivedFrames);
	printf("\n");
	printf("Timeouts:         %d\n", stats.timeouts);
	printf("\n");
	printf("Sent RR:          %d\n", stats.sentRR);
	printf("Received RR:      %d\n", stats.receivedRR);
	printf("\n");
	printf("Sent REJ:         %d\n", stats.sentREJ);
	printf("Received REJ:     %d\n", stats.receivedREJ);
	printf("\n");
	printf("Received bytes:   %d\n", stats.receivedBytes);
	printf("Sent bytes:      %d\n", stats.sentBytes);
	printf("FileSize:         %lu bytes\n", stats.fileSize);
	printf("\n");
	printf("Sent packets:     %d\n", stats.sentPackets);
	printf("Received packets: %d\n", stats.receivedPackets);
}

Statistics initStatistics() {
	Statistics stats;

	stats.sentFrames = 0;
	stats.receivedFrames = 0;

	stats.timeouts = 0;

	stats.sentRR = 0;
	stats.receivedRR = 0;

	stats.sentREJ = 0;
	stats.receivedREJ = 0;

	stats.sentPackets = 0;
	stats.receivedPackets = 0;

	stats.sentBytes = 0;
	stats.receivedBytes = 0;
	stats.fileSize = 0;

	return stats;
}

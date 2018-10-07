#include "pch.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include "MultiplicativePRNG.h"
#include "NormalModel.h"
#include "LaplaceModel.h"
#include "ExponentialModel.h"

using namespace std;

FILE* outputFile;


const char* printBool(bool value)
{
	return (value) ? "true" : "false";
}

double calcErf(double from, double to, int stepNum)
{
	double step = (to - from) / stepNum;
	double sum = (1.0 + exp(-to * to)) / 2.0;

	for (int i = 1; i < stepNum; i++)
	{
		sum += exp(-pow(from + i * step, 2.0));
	}

	return 2.0 * step * sum / sqrt(M_PI);
}

double calcErf(double max, int stepNum)
{
	return calcErf(0.0, max, stepNum);
}


double calcEstimationMean(const double* source, int num)
{
	double sum = 0.0;

	for (int i = 0; i < num; i++)
	{
		sum += source[i];
	}

	return sum / num;
}

double calcEstimationVariance(const double* source, int num, double mean)
{
	double sum = 0.0;

	for (int i = 0; i < num; i++)
	{
		sum += pow(source[i] - mean, 2.0);
	}

	return sum / (num - 1);
}

double calcNormalCDF(double x, double mean, double variance, int erfStepNum)
{
	return (1.0 + calcErf((x - mean) / sqrt(2.0 * variance), erfStepNum)) / 2.0;
}

double calcLaplaceMean()
{
	return 0.0;
}

double calcLaplaceVariance(double lambda)
{
	return 2.0 / (lambda * lambda);
}

double calcLaplaceCDF(double x, double lambda)
{
	return (x < 0) ? exp(lambda * x) / 2.0 : 1.0 - exp(-lambda * x) / 2.0;
}

double calcExponentialMean(double lambda)
{
	return 1.0 / lambda;
}

double calcExponentialVariance(double lambda)
{
	return 1.0 / (lambda * lambda);
}

double calcExponentialCDF(double x, double lambda)
{
	return 1.0 - exp(-lambda * x);
}

int* calcFrequenciesEmperic(double* sequence, int num, int cellNum, double leftBorder, double rightBorder)
{
	int* result = new int[cellNum];
	double step = (rightBorder - leftBorder) / cellNum;
	double curBorder = step;
	int resultIndex = 0;

	for (int i = 0; i < cellNum; i++)
	{
		result[i] = 0.0;
	}

	for (int i = 0; i < num; i++)
	{
		if (sequence[i] < curBorder)
		{
			result[resultIndex]++;
		}
		else if (resultIndex < cellNum - 1)
		{
			resultIndex++;
			curBorder += step;
		}
	}

	return result;
}

bool checkPearsonTestNormal(const double* quantiles, double* sequence, int num, int cellNum, int erfStepNum, double mean, double variance)
{
	int* empericFreq;
	double chi = 0.0;
	double expectedCount;
	double step;
	double curBorder;
	double prevBorder;
	double denom = sqrt(2.0 * variance);

	sort(sequence, &sequence[num]);
	empericFreq = calcFrequenciesEmperic(sequence, num, cellNum, sequence[0], sequence[num - 1]);

	step = (sequence[num - 1] - sequence[0]) / cellNum;
	curBorder = sequence[0];

	for(int i = 0; i < cellNum; i++)
	{
		prevBorder = curBorder;
		curBorder = (i + 1) * step;
		expectedCount = num * (calcErf((prevBorder - mean) / denom, (curBorder - mean) / denom, erfStepNum) / 2.0);
		chi += pow(empericFreq[i] - expectedCount, 2.0) / expectedCount;
	}

	delete[] empericFreq;

	printf("Chi: %f\n", chi);
	fprintf(outputFile, "Chi: %f\n", chi);
	printf("Quantile: %f\n", quantiles[cellNum - 2]);
	fprintf(outputFile, "Quantile: %f\n", quantiles[cellNum - 2]);

	return (chi < quantiles[cellNum - 2]);
}

bool checkPearsonTestLaplace(const double* quantiles, double* sequence, int num, int cellNum, double lambda)
{
	int* empericFreq;
	double chi = 0.0;
	double expectedCount;
	double step;
	double curBorder;
	double prevBorder;

	sort(sequence, &sequence[num]);
	empericFreq = calcFrequenciesEmperic(sequence, num, cellNum, sequence[0], sequence[num - 1]);

	step = (sequence[num - 1] - sequence[0]) / cellNum;
	curBorder = sequence[0];

	for(int i = 0; i < cellNum; i++)
	{
		prevBorder = curBorder;
		curBorder = (i + 1) * step;
		expectedCount = num * (calcLaplaceCDF(curBorder, lambda) - calcLaplaceCDF(prevBorder, lambda));
		chi += pow(empericFreq[i] - expectedCount, 2.0) / expectedCount;
	}

	delete[] empericFreq;

	printf("Chi: %f\n", chi);
	fprintf(outputFile, "Chi: %f\n", chi);
	printf("Quantile: %f\n", quantiles[cellNum - 2]);
	fprintf(outputFile, "Quantile: %f\n", quantiles[cellNum - 2]);

	return (chi < quantiles[cellNum - 2]);
}

bool checkPearsonTestExponential(const double* quantiles, double* sequence, int num, int cellNum, double lambda)
{
	int* empericFreq;
	double chi = 0.0;
	double expectedCount;
	double step;
	double curBorder;
	double prevBorder;

	sort(sequence, &sequence[num]);
	empericFreq = calcFrequenciesEmperic(sequence, num, cellNum, sequence[0], sequence[num - 1]);

	step = (sequence[num - 1] - sequence[0]) / cellNum;
	curBorder = sequence[0];

	for(int i = 0; i < cellNum; i++)
	{
		prevBorder = curBorder;
		curBorder = (i + 1) * step;
		expectedCount = num * (calcExponentialCDF(curBorder, lambda) - calcExponentialCDF(prevBorder, lambda));
		chi += pow(empericFreq[i] - expectedCount, 2.0) / expectedCount;
	}

	delete[] empericFreq;

	printf("Chi: %f\n", chi);
	fprintf(outputFile, "Chi: %f\n", chi);
	printf("Quantile: %f\n", quantiles[cellNum - 2]);
	fprintf(outputFile, "Quantile: %f\n", quantiles[cellNum - 2]);

	return (chi < quantiles[cellNum - 2]);
}

double calcKolmogorovDistanceNormal(double* sequence, int num, int erfStepNum, double mean, double variance)
{
	double result = 0.0;
	for (int i = 0; i < num; i++)
	{
		result = max(result, abs(calcNormalCDF(sequence[i], mean, variance, erfStepNum) - (double) (i + 1) / num));
	}
	return result;
}

bool checkKolmogorovTestNormal(double quantile, double* sequence, int num, int erfStepNum, double mean, double variance)
{
	double distance;
	sort(sequence, &sequence[num]);
	distance = sqrt(num) * calcKolmogorovDistanceNormal(sequence, num, erfStepNum, mean, variance);
	printf("Kolmogorov distance: %lf\n", distance);
	fprintf(outputFile, "Kolmogorov distance: %lf\n", distance);
	printf("Quantile: %f\n", quantile);
	fprintf(outputFile, "Quantile: %f\n", quantile);
	return (distance < quantile);
}

double calcKolmogorovDistanceLaplace(double* sequence, int num, double lambda)
{
	double result = 0.0;
	for (int i = 0; i < num; i++)
	{
		result = max(result, abs(calcLaplaceCDF(sequence[i], lambda) - (double) (i + 1) / num));
	}
	return result;
}

bool checkKolmogorovTestLaplace(double quantile, double* sequence, int num, double lambda)
{
	double distance;
	sort(sequence, &sequence[num]);
	distance = sqrt(num) * calcKolmogorovDistanceLaplace(sequence, num, lambda);
	printf("Kolmogorov distance: %lf\n", distance);
	fprintf(outputFile, "Kolmogorov distance: %lf\n", distance);
	printf("Quantile: %f\n", quantile);
	fprintf(outputFile, "Quantile: %f\n", quantile);
	return (distance < quantile);
}

double calcKolmogorovDistanceExponential(double* sequence, int num, double lambda)
{
	double result = 0.0;
	for (int i = 0; i < num; i++)
	{
		result = max(result, abs(calcExponentialCDF(sequence[i], lambda) - (double) (i + 1) / num));
	}
	return result;
}

bool checkKolmogorovTestExponential(double quantile, double* sequence, int num, double lambda)
{
	double distance;
	sort(sequence, &sequence[num]);
	distance = sqrt(num) * calcKolmogorovDistanceExponential(sequence, num, lambda);
	printf("Kolmogorov distance: %lf\n", distance);
	fprintf(outputFile, "Kolmogorov distance: %lf\n", distance);
	printf("Quantile: %f\n", quantile);
	fprintf(outputFile, "Quantile: %f\n", quantile);
	return (distance < quantile);
}


int main()
{
	const double quantilesPearson[20] = {3.8415, 5.9915, 7.8147, 9.4877, 11.07, 12.592, 14.067, 15.507, 16.919, 18.307, 
								  19.675, 21.026, 22.362, 23.685, 24.996, 26.296, 27.587, 28.869, 30.144, 31.41};
	const double quantileKolmogorov = 1.36;
    const int num = 1000;
	const int cellNum = 20;
	const int erfStepNum = 200;
	const double normalMean = -5.0;
	const double normalVariance = 25.0;
	const double lambdaLaplace = 1.0;
	const double lambdaExponential = 4.0;
 
	PRNG* prng = new MultiplicativePRNG(pow(2, 31), 262147, 262147);
	NormalModel* normalModel = new NormalModel(prng, normalMean, normalVariance);
	LaplaceModel* laplaceModel = new LaplaceModel(prng, lambdaLaplace);
	ExponentialModel* exponentialModel = new ExponentialModel(prng, lambdaExponential);
 
	fopen_s(&outputFile, "output.txt", "w");
 
	double* resultNormal = new double[num];
	double* resultLaplace = new double[num];
	double* resultExponential = new double[num];
 
	double mean;
	double variance;
	bool testResult;
 
	delete prng;
 
	for (int i = 0; i < num; i++)
	{
		resultNormal[i] = normalModel->next();
		resultLaplace[i] = laplaceModel->next();
		resultExponential[i] = exponentialModel->next();
	}
 
	printf("Normal model (%lf, %lf):\n", normalMean, normalVariance);
	fprintf(outputFile, "Normal model (%lf, %lf):\n", normalMean, normalVariance);
	for (int i = 0; i < num; i++)
	{
		fprintf(outputFile, "%lf\n", resultNormal[i]);
	}
	mean = calcEstimationMean(resultNormal, num);
	variance = calcEstimationVariance(resultNormal, num, mean);
	printf("Mean: %f\n", mean);
	fprintf(outputFile, "Mean: %f\n", mean);
	printf("Variance: %f\n", variance);
	fprintf(outputFile, "Variance: %f\n", variance);
	mean = normalMean;
	variance = normalVariance;
	printf("Real mean: %f\n", mean);
	fprintf(outputFile, "Real mean: %f\n", mean);
	printf("Real variance: %f\n", variance);
	fprintf(outputFile, "Real variance: %f\n", variance);
	testResult = checkPearsonTestNormal(quantilesPearson, resultNormal, num, cellNum, erfStepNum, normalMean, normalVariance);
	printf("Pearson test: %s\n", printBool(testResult));
	fprintf(outputFile, "Pearson test: %s\n", printBool(testResult));
	testResult = checkKolmogorovTestNormal(quantileKolmogorov, resultNormal, num, erfStepNum, normalMean, normalVariance);
	printf("Kolmogorov test: %s\n", printBool(testResult));
	fprintf(outputFile, "Kolmogorov test: %s\n", printBool(testResult));
 
	printf("\nLaplace model (%lf):\n", lambdaLaplace);
	fprintf(outputFile, "\nLaplace model (%lf):\n", lambdaLaplace);
	for (int i = 0; i < num; i++)
	{
		fprintf(outputFile, "%lf\n", resultLaplace[i]);
	}
	mean = calcEstimationMean(resultLaplace, num);
	variance = calcEstimationVariance(resultLaplace, num, mean);
	printf("Mean: %f\n", mean);
	fprintf(outputFile, "Mean: %f\n", mean);
	printf("Variance: %f\n", variance);
	fprintf(outputFile, "Variance: %f\n", variance);
	mean = calcLaplaceMean();
	variance = calcLaplaceVariance(lambdaLaplace);
	printf("Real mean: %f\n", mean);
	fprintf(outputFile, "Real mean: %f\n", mean);
	printf("Real variance: %f\n", variance);
	fprintf(outputFile, "Real variance: %f\n", variance);
	testResult = checkPearsonTestLaplace(quantilesPearson, resultLaplace, num, cellNum, lambdaLaplace);
	printf("Pearson test: %s\n", printBool(testResult));
	fprintf(outputFile, "Pearson test: %s\n", printBool(testResult));
	testResult = checkKolmogorovTestLaplace(quantileKolmogorov, resultLaplace, num, lambdaLaplace);
	printf("Kolmogorov test: %s\n", printBool(testResult));
	fprintf(outputFile, "Kolmogorov test: %s\n", printBool(testResult));

	printf("\nExponential model (%lf):\n", lambdaExponential);
	fprintf(outputFile, "\nExponential model (%lf):\n", lambdaExponential);
	for (int i = 0; i < num; i++)
	{
		fprintf(outputFile, "%lf\n", resultExponential[i]);
	}
	mean = calcEstimationMean(resultExponential, num);
	variance = calcEstimationVariance(resultExponential, num, mean);
	printf("Mean: %f\n", mean);
	fprintf(outputFile, "Mean: %f\n", mean);
	printf("Variance: %f\n", variance);
	fprintf(outputFile, "Variance: %f\n", variance);
	mean = calcExponentialMean(lambdaExponential);
	variance = calcExponentialVariance(lambdaExponential);
	printf("Real mean: %f\n", mean);
	fprintf(outputFile, "Real mean: %f\n", mean);
	printf("Real variance: %f\n", variance);
	fprintf(outputFile, "Real variance: %f\n", variance);
	testResult = checkPearsonTestExponential(quantilesPearson, resultExponential, num, cellNum, lambdaExponential);
	printf("Pearson test: %s\n", printBool(testResult));
	fprintf(outputFile, "Pearson test: %s\n", printBool(testResult));
	testResult = checkKolmogorovTestExponential(quantileKolmogorov, resultExponential, num, lambdaExponential);
	printf("Kolmogorov test: %s\n", printBool(testResult));
	fprintf(outputFile, "Kolmogorov test: %s\n", printBool(testResult));
 
	fclose(outputFile);
 
	delete normalModel;
	delete laplaceModel;
	delete exponentialModel;
	delete[] resultNormal;
	delete[] resultLaplace;
	delete[] resultExponential;
}

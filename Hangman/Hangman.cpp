// Hangman.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <algorithm>
#include <functional>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <random>
#include <fstream>
#include <iostream>
#include <cmath>
#include <ctime>

using namespace std;

const unsigned seed = time(NULL);
default_random_engine generator(seed);

normal_distribution<double> normal;
uniform_int_distribution<int> uniform;
uniform_real_distribution<double> uniformR;

#define STD_NORMAL normal(generator)
#define INT_UNIFORM uniform(generator)
#define REAL_UNIFORM uniformR(generator)

struct Activation
{
	struct Rectifier
	{
		static double eval(double x)
		{
			return x >= 0 ? x : 0;
		}
		static double deriv(double x)
		{
			return x >= 0 ? 1 : 0;
		}
	};
	struct Logistic
	{
		static double eval(double x)
		{
			return 1 / (1 + exp(-x));
		}
		static double deriv(double x)
		{
			const double u = exp(-x);
			return u / ((1 + u) * (1 + u));
		}
	};
	struct HypTangent
	{
		static double eval(double x)
		{
			return tanh(x);
		}
		static double deriv(double x)
		{
			const double u = cosh(x);
			return 1 / (u * u);
		}
	};
};

template<typename Func>
class NeuralNetwork
{
protected:
	vector<size_t> layers;
	vector<vector<vector<double>>>
		weights, errors;
	vector<vector<double>> inputs, outputs;
	const double wMax = INT_MAX, wMin = INT_MIN;
	int count = 0;
public:
	NeuralNetwork(const NeuralNetwork &n) :
		layers(n.layers), weights(n.weights)
	{}
	NeuralNetwork(const vector<size_t> &s) :
		layers(s), weights(s.size() - 1)
	{
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			weights[i] = vector<vector<double>>(
				layers[i + 1], vector<double>(layers[i] + 1));
		}
		errors = weights;
	}
	NeuralNetwork(const char name[])
	{
		ifstream file(name, ios::in);
		size_t size; file >> size;
		layers = vector<size_t>(size);
		weights = vector<vector<vector<double>>>(size - 1);
		for (int i = 0; i < size; ++i)
		{
			file >> layers[i];
		}
		for (int i = 0; i < size - 1; ++i)
		{
			weights[i] = vector<vector<double>>(
				layers[i + 1], vector<double>(layers[i] + 1));
		}
		errors = weights;
		for (int i = 0; i < size - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					file >> weights[i][j][k];
				}
			}
		}
		file.close();
	}
	void randomize()
	{
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					weights[i][j][k] = STD_NORMAL / 3;
					weights[i][j][k] = clamp(weights[i][j][k], wMin, wMax);
				}
			}
		}
	}
	void mutate(double rate, double step)
	{
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					if (REAL_UNIFORM < rate)
					{
						weights[i][j][k] += step * STD_NORMAL;
						weights[i][j][k] = clamp(weights[i][j][k], wMin, wMax);
					}
				}
			}
		}
	}
	void crossover(const NeuralNetwork &other)
	{
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			int row = INT_UNIFORM % layers[i + 1],
				column = INT_UNIFORM % (layers[i] + 1);
			for (int j = 0; j < row; ++j)
			{
				weights[i][j] = other.weights[i][j];
			}
			for (int j = 0; j < column; ++j)
			{
				weights[i][row][j] = other.weights[i][row][j];
			}
		}
	}
	void update(double rate)
	{
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					weights[i][j][k] -= rate * errors[i][j][k] / count;
					errors[i][j][k] = 0;
				}
			}
		}
		count = 0;
	}
	vector<double> think(const vector<double> &input)
	{
		outputs = vector<vector<double>>(layers.size());
		outputs[0] = vector<double>(input);
		for (int i = 1; i < layers.size(); ++i)
		{
			outputs[i] = vector<double>(layers[i]);
		}
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			outputs[i].push_back(1);
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				double sum = 0;
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					sum += outputs[i][k] * weights[i][j][k];
				}
				outputs[i + 1][j] = Func::eval(sum);
			}
		}
		return outputs.back();
	}
	void propagate(const vector<double> &expected)
	{
		vector<vector<double>> delta(layers.size());
		delta.back() = vector<double>(layers.back());
		for (int i = 0; i < layers.back(); ++i)
		{
			double output = outputs.back()[i];
			double sens = Func::deriv(inputs.back()[i]);
			delta.back()[i] = sens * (output - expected[i]);
		}
		for (int i = layers.size() - 2; i > 0; --i)
		{
			delta[i] = vector<double>(layers[i]);
			for (int j = 0; j < layers[i]; ++j)
			{
				double output = outputs[i][j];
				double sens = Func::deriv(inputs[i][j]);
				for (int k = 0; k < layers[i + 1]; ++k)
				{
					delta[i][j] += sens *
						delta[i + 1][k] * weights[i][k][j];
				}
			}
		}
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					errors[i][j][k] += outputs[i][k] * delta[i + 1][j];
				}
			}
		}
		++count;
	}
	void save(const char name[]) const
	{
		ofstream file(name, ios::out | ios::trunc);
		file << layers.size() << ' ';
		for (int i = 0; i < layers.size(); ++i)
		{
			file << layers[i] << ' ';
		}
		file << endl;
		for (int i = 0; i < layers.size() - 1; ++i)
		{
			for (int j = 0; j < layers[i + 1]; ++j)
			{
				for (int k = 0; k < layers[i] + 1; ++k)
				{
					file << weights[i][j][k] << ' ';
				}
				file << endl;
			}
			file << endl;
		}
		file << endl;
		file.close();
	}
};

template<>
void NeuralNetwork<Activation::Logistic>::
	propagate(const vector<double> &expected)
{
	vector<vector<double>> delta(layers.size());
	delta.back() = vector<double>(layers.back());
	for (int i = 0; i < layers.back(); ++i)
	{
		double output = outputs.back()[i];
		double sens = output * (1 - output);
		delta.back()[i] = sens * (output - expected[i]);
	}
	for (int i = layers.size() - 2; i > 0; --i)
	{
		delta[i] = vector<double>(layers[i]);
		for (int j = 0; j < layers[i]; ++j)
		{
			double output = outputs[i][j];
			double sens = output * (1 - output);
			for (int k = 0; k < layers[i + 1]; ++k)
			{
				delta[i][j] += sens *
					delta[i + 1][k] * weights[i][k][j];
			}
		}
	}
	for (int i = 0; i < layers.size() - 1; ++i)
	{
		for (int j = 0; j < layers[i + 1]; ++j)
		{
			for (int k = 0; k < layers[i] + 1; ++k)
			{
				errors[i][j][k] += outputs[i][k] * delta[i + 1][j];
			}
		}
	}
	++count;
}

#define WORDS 370104

class Hangman
{
	string word = "", display = "";
public:
	static array<string, WORDS> vocab;
	Hangman(const string &w) : word(w)
	{
		display = string(word.size(), ' ');
	}
	Hangman() : word(vocab[INT_UNIFORM % WORDS])
	{
		display = string(word.size(), ' ');
	}
	Hangman(int minLength, int maxLength) :
		word(vocab[INT_UNIFORM % WORDS])
	{
		while (word.size() < minLength ||
			word.size() > maxLength)
		{
			word = vocab[INT_UNIFORM % WORDS];
		}
		display = string(word.size(), ' ');
	}
	string getAnswer() const { return word; }
	string getDisplay() const { return display; }
	static void loadVocab(const char name[])
	{
		ifstream file(name, ios::in);
		for (int i = 0; i < WORDS; ++i)
		{
			file >> vocab[i];
		}
		file.close();
	}
	bool check(char letter)
	{
		bool found = false;
		for (int i = 0; i < word.size(); ++i)
		{
			if (word[i] == letter)
			{
				display[i] = word[i];
				found = true;
			}
		}
		return found;
	}
};

array<string, WORDS> Hangman::vocab;

class Player
{
	NeuralNetwork<Activation::Logistic> brain;
public:
	Player(const char name[]) : brain(name)
	{}
	Player(const vector<size_t> &s) : brain(s)
	{
		brain.randomize();
	}
	void saveBrain(const char name[]) const
	{
		brain.save(name);
	}
	void crossBrain(const Player &other)
	{
		brain.crossover(other.brain);
	}
	void mutateBrain(double rate, double step)
	{
		brain.mutate(rate, step);
	}
	void updateBrain(double rate)
	{
		brain.update(rate);
	}
	double play(Hangman game, int maxMisses = 26)
	{
		string word = game.getAnswer();
		vector<int> letters(26);
		vector<bool> guessed(26);
		int count = 0, guesses = 0;
		for (char l : word)
			++letters[l - 97];
		while (game.getDisplay() != word)
		{
			string display = game.getDisplay();
			vector<double> expected(26);
			const int remain = word.size() - count;
			for (int i = 0; i < 26; ++i)
			{
				expected[i] = tanh(4.0 * letters[i] / remain);
			}
			list<char> letters = guess(
				display, guessed, expected);
			++guesses;
			while (!game.check(letters.front()))
			{
				//cout << letters.front() << ' ' << display << endl;
				guessed[letters.front() - 97] = true;
				letters.pop_front();
				if (guesses - count == maxMisses) return 0;
				++guesses;
			}
			//cout << letters.front() << ' ' << game.getDisplay() << endl;
			guessed[letters.front() - 97] = true;
			++count;
		}
		return (double)count / guesses;
	}
	list<char> guess(string display, vector<bool> guessed,
		const vector<double> &expected)
	{
		const int inputSize = 432;
		vector<double> input(inputSize);
		for (int i = 0; i < display.size(); ++i)
		{
			if (i * 27 == inputSize) break;
			if (display[i] != ' ')
				input[i * 27 + display[i] - 97] = 1;
			input[i * 27 + 26] = 1;
		}
		vector<double> output = brain.think(input);
		list<char> letters;
		for (int i = 0; i < 26; ++i)
		{
			if (guessed[i]) continue;
			list<char>::iterator iter;
			for (iter = letters.begin();
				iter != letters.end(); ++iter)
			{
				if (output[i] >= output[*iter - 97])
					break;
			}
			letters.insert(iter, i + 97);
		}
		brain.propagate(expected);
		return letters;
	}
};

class Population
{
	vector<Player> players;
	const vector<size_t> layers =
	{ 432, 229, 26 };
	const double rate = 0.01, step = 0.25;
	const int gameSize = 10;
public:
	Population(int size)
	{
		for (int i = 0; i < size; ++i)
		{
			players.emplace_back(layers);
		}
	}
	Population(int size, const char name[])
	{
		players.emplace_back(name);
		for (int i = 1; i < size; ++i)
		{
			players.emplace_back(players.front());
			players.back().mutateBrain(rate, step);
		}
	}
	double score(double p) { return exp2(10 * p); }
	void evolve(const char name[], int maxGen = INT_MAX)
	{
		for (int i = 0; i < maxGen; ++i)
		{
			vector<Hangman> games(gameSize);
			vector<double> scores(players.size());
			double sumScore = 0, maxScore = -INFINITY;
			int best = -1;
			for (int j = 0; j < gameSize; ++j)
			{
				games[j] = Hangman(4, 16);
			}
			for (int j = 0; j < players.size(); j++)
			{
				system("cls");
				cout << "Gen=" << i + 1 << endl;
				cout << "Player=" << j + 1 << endl;
				double accr = 0;
				for (int k = 0; k < gameSize; ++k)
				{
					cout << games[k].getAnswer() << endl;
					accr += players[j].play(games[k]) / gameSize;
				}
				scores[j] = score(accr);
				sumScore += scores[j];
				if (scores[j] > maxScore)
				{
					best = j;
					maxScore = scores[j];
				}
				cout << "Score=" << scores[j] << endl;
				cout << "MaxScore=" << maxScore << endl;
			}
			if ((i + 1) % 10 == 0)
				players[best].saveBrain(name);
			players = generate(best, sumScore, scores);
		}
	}
	vector<Player> generate(int best, double sum,
		const vector<double> &scores) const
	{
		vector<Player> newPlayers;
		newPlayers.emplace_back(players[best]);
		for (int i = 1; i < players.size(); ++i)
		{
			double val = REAL_UNIFORM * sum;
			double current = 0;
			for (int j = 0; j < players.size(); ++j)
			{
				current += scores[j];
				if (current >= val)
				{
					newPlayers.emplace_back(players[j]);
					break;
				}
			}
			if (i > players.size() / 2)
			{
				val = REAL_UNIFORM * sum;
				current = 0;
				for (int j = 0; j < players.size(); ++j)
				{
					current += scores[j];
					if (current >= val)
					{
						newPlayers.back().crossBrain(players[j]);
						break;
					}
				}
			}
			newPlayers.back().mutateBrain(rate, step);
		}
		return newPlayers;
	}
};

void test(const char name[], int games = INT_MAX)
{
	Player player(name);
	double perf = 0;
	for (int i = 0; i < games; ++i)
	{
		/*string word; cin >> word;
		Hangman game(word);*/
		Hangman game(1, 16);
		double accr = player.play(game);
		perf += accr / games;
		cout << game.getAnswer() << ' ' << accr << endl;
		//system("pause");
	}
	cout << perf << endl;
}

void train(const char name[], int epochs = INT_MAX,
	int batchSize = 100, double alpha = 1.0)
{
	Player player({ 432, 229, 26 });
	double prev = 0, curr = 0;
	const int freq = 100000 / batchSize;
	//player.loadBrain("player9.txt");
	for (int i = 0; i < epochs; ++i)
	{
		double accr = 0;
		for (int j = 0; j < batchSize; ++j)
		{
			Hangman game(1, 16);
			accr += player.play(game) / batchSize;
		}
		player.updateBrain(alpha);
		cout << i << ' ' << alpha << ' ' <<
			prev << ' ' << accr << endl;
		curr += accr / freq;
		if ((i + 1) % freq == 0)
		{
			player.saveBrain(name);
			if (curr < prev) alpha /= 2; else prev = curr;
			curr = 0;
		}
	}
}

int main()
{
	Hangman::loadVocab("words_alpha.txt");
	test("player9.txt", 10000);
	//train("player10.txt", 25000);
	/*Population population(100);
	population.evolve("player.txt", 1000);*/
	return 0;
}

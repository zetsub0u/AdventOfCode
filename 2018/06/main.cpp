#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define UNDEFINED_INDEX 0xFFFFFFFF

#define ABS(x) ((x > 0) ? (x) : -(x))
#define MINIMUM(a, b) ((a < b) ? (a) : (b))
#define MAXIMUM(a, b) ((a > b) ? (a) : (b))

struct v2 {
	int X;
	int Y;
};

struct rect {
	v2 LowerLeft;
	v2 UpperRight;
};

v2 V2(int X, int Y) {
	v2 Result = { X, Y };
	return Result;
}

v2 operator-(const v2 &A, const v2 &B) {
	v2 D = { A.X - B.X, A.Y - B.Y };
	return D;
}

int GetDistance(v2 A, v2 B) {
	v2 D = A - B;
	int Sum = ABS(D.X) + ABS(D.Y);
	return Sum;
}

rect FindBoundingBox(v2 * Coordinates, int Count) {
	rect Result = {};
	if (Count) {
		Result.LowerLeft = Result.UpperRight = Coordinates[0];
		for (int i = 1; i < Count; i++) {
			Result.LowerLeft.X = MINIMUM(Result.LowerLeft.X, Coordinates[i].X);
			Result.LowerLeft.Y = MINIMUM(Result.LowerLeft.Y, Coordinates[i].Y);
			Result.UpperRight.X = MAXIMUM(Result.UpperRight.X, Coordinates[i].X);
			Result.UpperRight.Y = MAXIMUM(Result.UpperRight.Y, Coordinates[i].Y);
		}
		Result.UpperRight.X++;
		Result.UpperRight.Y++;
	}
	return Result;
}

#define MAX_COUNT 256
struct file_content {
	int CoordinateCount;
	v2 Coordindates[MAX_COUNT];
};

file_content ReadInputFile(const char * path) {
	file_content Content = {};
	FILE *f = fopen(path, "r");
	if (f) {
		v2 Tmp = {};
		while (fscanf(f, "%d, %d\n", &Tmp.X, &Tmp.Y) == 2) {
			if (Content.CoordinateCount < MAX_COUNT) {
				Content.Coordindates[Content.CoordinateCount++] = Tmp;
			}
			else {
				fprintf(stderr, "Not enough memory\n");
				exit(1);
			}
		}
		fclose(f);
	}
	else {
		fprintf(stderr, "Could not open file %s\n", path);
		exit(1);
	}
	return Content;
}

struct tile {
	int Index;
	int Distance;
};

uint32_t PackBGRA(uint8_t B, uint8_t G, uint8_t R, uint8_t A) {
	uint32_t Result = (((((A << 8) | R) << 8) | G) << 8) | B;
	return Result;
}

#pragma pack(push, 1)
struct bitmap_header {
	uint16_t Magic;
	uint32_t Size;
	uint16_t Reserved1;
	uint16_t Reserved2;
	uint32_t Offset;
	uint32_t InfoHeaderSize;
	uint32_t Width;
	uint32_t Height;
	uint16_t Planes;
	uint16_t BitsPerPixel;
	uint32_t Compression;
	uint32_t ImageSize;
	uint32_t Unused[4];
};
#pragma pack(pop)

void WriteBitmap(tile * Tiles, v2 Span, uint32_t * Colors, int Count, const char * Filename) {
	uint32_t InvalidColor = 0x00FFFFFF;
	FILE *f = fopen(Filename, "wb");
	if(f) {
		uint32_t TotalFileSize = sizeof(bitmap_header) + 4 * Span.X * Span.Y;
		bitmap_header Header = {};
		Header.Magic = 0x4D42;
		Header.Size = TotalFileSize;
		Header.Offset = sizeof(Header);
		Header.InfoHeaderSize = 40;
		Header.Width = Span.X;
		Header.Height = Span.Y;
		Header.Planes = 1;
		Header.BitsPerPixel = 32;
		Header.ImageSize = 4 * Span.X * Span.Y;
		fwrite(&Header, sizeof(Header), 1, f);
		uint32_t * Pixels = (uint32_t*)malloc(4 * Span.X * Span.Y);
		for (int y = 0; y < Span.Y; y++) {
			tile * Row = Tiles + (Span.Y - y - 1) * Span.X;
			for (int x = 0; x < Span.X; x++) {
				tile * Tile = Row + x;
				uint32_t Color = (Tile->Index == UNDEFINED_INDEX) ? InvalidColor : Colors[Tile->Index];
				Pixels[y * Span.X + x] = Color;
			}
		}
		fwrite(Pixels, 4, Span.X * Span.Y, f);
		fclose(f);
	}
	else {
		fprintf(stderr, "Could not open file %s for writing bmp\n", Filename);
	}
}

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

int main(int argc, char *argv[]) {
	LARGE_INTEGER Freq, Start, End;
	QueryPerformanceFrequency(&Freq);
	QueryPerformanceCounter(&Start);
#if 1
	file_content Content = ReadInputFile("input.txt");
#else
	file_content Content;
	v2 TestCoordinates[6] = {
		1, 1,
		1, 6,
		8, 3,
		3, 4,
		5, 5,
		8, 9
	};
	Content.CoordinateCount = 6;
	memcpy(Content.Coordindates, TestCoordinates, sizeof(TestCoordinates));
#endif
	rect Bounds = FindBoundingBox(Content.Coordindates, Content.CoordinateCount);
	v2 Span = Bounds.UpperRight - Bounds.LowerLeft;
	int TileCount = Span.X * Span.Y;
	tile * Tiles = (tile*)malloc(TileCount * sizeof(*Tiles));
	if (Tiles) {
		for (int i = 0; i < TileCount; i++) {
			Tiles[i].Distance = INT_MAX;
			Tiles[i].Index = UNDEFINED_INDEX;
		}
		for (int Index = 0; Index < Content.CoordinateCount; Index++) {
			v2 Coordinate = Content.Coordindates[Index] - Bounds.LowerLeft;
			for (int Y = 0; Y < Span.Y; Y++) {
				for (int X = 0; X < Span.X; X++) {
					int Offset = Y * Span.X + X;
					tile * Tile = Tiles + Offset;
					int Distance = GetDistance(Coordinate, V2(X, Y));
					if (Distance == Tile->Distance) {
						Tile->Index = UNDEFINED_INDEX;
					}
					else if (Distance < Tile->Distance) {
						Tile->Index = Index;
						Tile->Distance = Distance;
					}
				}
			}
		}

		int * Bins = (int*)calloc(Content.CoordinateCount + 1, sizeof(*Bins));
		for (int i = 0; i < Span.X * Span.Y; i++) {
			tile * Tile = Tiles + i;
			int Index = (Tile->Index != UNDEFINED_INDEX) ? Tile->Index : Content.CoordinateCount;
			Bins[Index]++;
		}
		for (int X = 0; X < Span.X; X++) {
			Bins[Tiles[X].Index] = 0;
		}
		for (int X = 0; X < Span.X; X++) {
			Bins[Tiles[(Span.Y - 1) * Span.X + X].Index] = 0;
		}
		for (int Y = 0; Y < Span.Y; Y++) {
			Bins[Tiles[Y * Span.X].Index] = 0;
		}
		for (int Y = 0; Y < Span.Y; Y++) {
			Bins[Tiles[(Y + 1) * Span.X - 1].Index] = 0;
		}

		int Max = 0;
		int Index = UNDEFINED_INDEX;
		for (int i = 0; i < Content.CoordinateCount; i++) {
			v2 P = Content.Coordindates[i];
			if (P.X == Bounds.LowerLeft.X || P.X + 1 == Bounds.UpperRight.X || P.Y == Bounds.LowerLeft.Y || P.Y == Bounds.UpperRight.Y)
				continue;
			if (Bins[i] > Max) {
				Max = Bins[i];
				Index = i;
			}
		}

		QueryPerformanceCounter(&End);
		fprintf(stdout, "Largest area at index %d at coordinate (%d, %d): %d\n", Index, Content.Coordindates[Index].X, Content.Coordindates[Index].Y, Max);
		fprintf(stdout, "Time: %.3f ms\n", (1000.0 * (End.QuadPart - Start.QuadPart)) / (double)Freq.QuadPart);

#if 1
		FILE * outfile = fopen("out.txt", "w");
		if (outfile) {
			for (int Y = 0; Y < Span.Y; Y++) {
				for (int X = 0; X < Span.X; X++) {
					int Offset = Y * Span.X + X;
					tile * Tile = Tiles + Offset;
					fputc((Tile->Index == UNDEFINED_INDEX) ? '.' : Tile->Index + ((Tile->Distance == 0) ? 'A' : 'a'), outfile);
				}
				fputc('\n', outfile);
			}
			fclose(outfile);
		}

		uint32_t * Colors = (uint32_t*)malloc(4 * Content.CoordinateCount);
		for (int i = 0; i < Content.CoordinateCount; i++) {
			uint8_t R, G, B, A;
			R = rand() % 256;
			G = rand() % 256;
			B = rand() % 256;
			A = 0xFF;
			Colors[i] = PackBGRA(B, G, R, A);
		}

		WriteBitmap(Tiles, Span, Colors, Content.CoordinateCount, "out.bmp");
		fprintf(stdout, "Pixel: %d, %d\n", Content.Coordindates[Index].X - Bounds.LowerLeft.X, Content.Coordindates[Index].Y - Bounds.LowerLeft.Y);
#endif
	}
	else {
		fprintf(stderr, "Could not allocate memory\n");
		return 1;
	}

	return 0;
}
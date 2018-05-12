#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>

#define SYMBOL_RATE         72000.0
#define AGC_RATE            10.0

#define PLL_ALPHA           0.005
#define PLL_BETA            (0.05 * PLL_ALPHA * PLL_ALPHA)
#define PLL_LOCK_THRESHOLD  25.0

#define TIMING_ALPHA        0.25e-7
#define TIMING_BETA         1.5e-7

#define INPUT_BUFFER_SIZE           1024
#define OUTPUT_BUFFER_SIZE          1024
#define CONSTELLATION_SIZE          1024


#define ANSI_NO_COLOR   0
#define ANSI_BOLD       1
#define ANSI_FG_RED     31
#define ANSI_FG_GREEN   32


void term_reset() {
    fprintf(stderr, "\x1b""c");
}

void term_clear() {
    fprintf(stderr, "\x1b[2J");
}

void term_clear_line() {
    fprintf(stderr, "\x1b[2K");
}

void term_resize(int height, int width) {
    fprintf(stderr, "\x1b[8;%d;%dt", height, width);
}

void term_goto(int y, int x) {
    fprintf(stderr, "\x1b[%d;%dH", y, x);
}

void term_char(int y, int x, char c) {
    term_goto(y, x);
    fputc(c, stderr);
}

void term_color(int color) {
    fprintf(stderr, "\x1b[%dm", color);
}


// DSP stuff
double make_iir(double cutoff, double sample_rate) {
    return 1.0 - exp(-2.0 * M_PI * cutoff / sample_rate);
}

inline double slice(double x) {
    if (x < 0.0) {
        return -1.0;
    } else if (x > 0.0) {
        return 1.0;
    } else {
        return 0.0;
    }
}

inline char clamp(int x) {
    if (x < -128.0) {
        return -128.0;
    } else if (x > 127.0) {
        return 127.0;
    } else {
        return (char)x;
    }
}


int main(int argc, char *argv[]) {
    int sample_rate, pll_locked;
    size_t samples_read, i, j = 0;

    double agc_mean = 0.0, agc_coeff, filter_coeff, pll_freq = 0.0, pll_prev_freq = 0.0,
        pll_phase = 0.0, pll_error, sym_freq = 0.0, sym_phase = 0.0, sym_real, sym_prev_real = 0.0, sym_error;

    double complex input_sample, agc_output, filter_output = 0.0, pll_carrier, pll_output;

    char cx, cy;
    float complex input_buffer[INPUT_BUFFER_SIZE];
    char output_buffer[OUTPUT_BUFFER_SIZE];

    FILE *infile, *outfile;

    if (argc != 4 || !(sample_rate = atoi(argv[2]))) {
        fprintf(stderr, "Usage: %s <input file> <input rate> <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!(infile = fopen(argv[1], "r"))) {
        perror("Couldn't open the input file");
        return EXIT_FAILURE;
    }

    if (!(outfile = fopen(argv[3], "w"))) {
        perror("Couldn't open the output file");
        fclose(infile);
        return EXIT_FAILURE;
    }

    // Initialize the terminal
    term_reset();
    term_resize(36, 72);

    // Calculate the IIR coefficient for automatic gain control and the input filter
    agc_coeff = make_iir(AGC_RATE, sample_rate);
    filter_coeff = make_iir(SYMBOL_RATE * M_SQRT2 / 2.0, sample_rate);

    // Symbol clock frequency
    sym_freq = SYMBOL_RATE;

    while ((samples_read = fread(&input_buffer, sizeof(float complex), INPUT_BUFFER_SIZE, infile))) {
        for (i = 0; i < samples_read; i++) {
            input_sample = input_buffer[i];

            // Update the AGC mean level
            agc_mean += agc_coeff * (cabs(input_sample) - agc_mean);

            // Calculate the AGC output
            agc_output = input_sample / agc_mean / 2.0;

            // Calculate the input filter output
            filter_output += filter_coeff * (agc_output - filter_output);

            // Update the PLL
            pll_carrier = cexp(I * pll_phase);
            pll_output = filter_output * conj(pll_carrier);
            pll_error = carg(pll_output);
            pll_phase += pll_error * PLL_ALPHA;
            pll_freq += pll_error * PLL_BETA;

            pll_phase += pll_freq;
            pll_phase = fmod(pll_phase, M_PI * 2.0);

            // Symbol timing recovery and production of output samples (only when the PLL is locked)
            if (sym_phase > 1.0) {
                sym_real = creal(pll_output);

                sym_error = sym_real * slice(sym_prev_real) - sym_prev_real * slice(sym_real);

                sym_phase += sym_error * TIMING_ALPHA;
                sym_freq += sym_error * TIMING_BETA;
                sym_phase = fmod(sym_phase, 1.0);

                sym_prev_real = sym_real;

                // Write the output sample
                output_buffer[j++] = clamp(creal(pll_output) * 1.5 * 128.0);
                output_buffer[j++] = clamp(cimag(pll_output) * 1.5 * 128.0);

                // Flush the buffer
                if (j == OUTPUT_BUFFER_SIZE) {
                    j = 0;

                    if (fwrite(output_buffer, sizeof(char), OUTPUT_BUFFER_SIZE, outfile) < OUTPUT_BUFFER_SIZE) {
                        perror("Couldn't write the output file");
                        fclose(infile);
                        fclose(outfile);
                        return EXIT_FAILURE;
                    }
                }
            }

            sym_phase += sym_freq / sample_rate;
        }

        // Check if the PLL is locked
        if (fabs(pll_freq - pll_prev_freq) * sample_rate < PLL_LOCK_THRESHOLD) {
            pll_locked = 1;
        } else {
            pll_locked = 0;
        }

        // Render the display
        term_clear();
        term_goto(1, 0);
        term_color(ANSI_BOLD);
        term_color(ANSI_FG_GREEN);
        fprintf(stderr, "Meteor-M2 LRPT demodulator");
        term_color(ANSI_NO_COLOR);
        term_goto(2, 0);
        fprintf(stderr, "Symbol rate: %.5f\n", sym_freq);
        term_goto(3, 0);
        fprintf(stderr, "PLL frequency: ");
        term_color(ANSI_BOLD);
        term_color(pll_locked ? ANSI_FG_GREEN : ANSI_FG_RED);
        fprintf(stderr, "%.2f\n", pll_freq * sample_rate);
        term_color(ANSI_NO_COLOR);
        term_color(ANSI_BOLD);

        for (i = 0; i < CONSTELLATION_SIZE; i += 2) {
            cx = output_buffer[i];
            cy = output_buffer[i+1];

            term_char(20 + cy / 8, 36 + cx / 4, '#');
        }

        pll_prev_freq = pll_freq;

    }

    fclose(infile);
    fclose(outfile);

    // Restore the terminal
    term_resize(24, 80);
    term_reset();
}

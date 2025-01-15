#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdio.h>

// Configurações do LED e buzzer
#define LED_PIN 12          // Define o pino do LED
#define BUZZER_PIN 21       // Define o pino do buzzer
#define BUZZER_FREQUENCY 4000 // Frequência do buzzer (Hz)

// Configurações da rede Wi-Fi
#define WIFI_SSID "NomeDaRedeWiFi"  // Nome da rede Wi-Fi
#define WIFI_PASS "SenhaDaRedeWiFi" // Senha da rede Wi-Fi

// Buffer para respostas HTTP
#define HTTP_RESPONSE "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" \
                      "<!DOCTYPE html><html><body>" \
                      "<h1>Controle do LED e Buzzer via Wi-Fi</h1>" \
                      "<p><a href=\"/led_buzzer/on\">Ligar LED e Buzzer</a></p>" \
                      "<p><a href=\"/led_buzzer/off\">Desligar LED e Buzzer</a></p>" \
                      "</body></html>\r\n"

// Variáveis globais para controle
static bool led_buzzer_active = false; // Indica se o LED e o buzzer estão ativos

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

// Liga ou desliga o buzzer
void set_buzzer_state(uint pin, bool state) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    if (state) {
        pwm_set_gpio_level(pin, 2048); // Duty cycle de 50%
    } else {
        pwm_set_gpio_level(pin, 0);    // Duty cycle de 0%
    }
}

// Callback para processar requisições HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb); // Cliente fechou a conexão
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    if (strstr(request, "GET /led_buzzer/on")) {
        led_buzzer_active = true;  // Ativa LED e buzzer
    } else if (strstr(request, "GET /led_buzzer/off")) {
        led_buzzer_active = false; // Desativa LED e buzzer
        gpio_put(LED_PIN, 0);      // Certifica-se de que o LED está apagado
        set_buzzer_state(BUZZER_PIN, false); // Certifica-se de que o buzzer está desligado
    }

    // Envia a resposta HTTP
    tcp_write(tpcb, HTTP_RESPONSE, strlen(HTTP_RESPONSE), TCP_WRITE_FLAG_COPY);
    pbuf_free(p); // Libera o buffer recebido

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

// Setup do servidor HTTP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

int main() {
    stdio_init_all();
    sleep_ms(10000); // Aguarda inicialização
    printf("Iniciando servidor HTTP\n");

    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }

    uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Wi-Fi conectado! Endereço IP: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    printf("Acesse o site para controlar o LED e o buzzer.\n");

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT); // Configura o LED como saída

    pwm_init_buzzer(BUZZER_PIN); // Inicializa o PWM do buzzer

    start_http_server(); // Inicia o servidor HTTP

    while (true) {
        if (led_buzzer_active) {
            gpio_put(LED_PIN, 1);         // Liga o LED
            set_buzzer_state(BUZZER_PIN, true); // Liga o buzzer
            sleep_ms(500);               // Pausa de 500 ms
            gpio_put(LED_PIN, 0);         // Desliga o LED
            set_buzzer_state(BUZZER_PIN, false); // Desliga o buzzer
            sleep_ms(500);               // Pausa de 500 ms
        }
        cyw43_arch_poll(); // Mantém o Wi-Fi ativo
    }

    cyw43_arch_deinit(); // Finaliza o Wi-Fi (nunca será chamado neste caso)
    return 0;
}

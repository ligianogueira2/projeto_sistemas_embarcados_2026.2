<h1 align="center"> Sistemas Embarcados 2026.2 </h1>
<p align="center"> 
<img src="src\img\foto_robo.png" alt="Imagem inicial" width="600" height="300" />
</p>
<h4 align="center"> Projeto Final - Disciplina de Sistemas Embarcados 2026.2 </h4>
<h4 align="center"> Engenharia de Computação/<a href="https://www.ifpb.edu.br/">IFPB</a> (ago - dez 2026) </h4>

<br>
</br>
<p align="center"> 
<a href="https://image.jimcdn.com/app/cms/image/transf/dimension=970x10000:format=gif/path/sa16dc2497d80e05e/image/icd162bb94ffa0064/version/1551588419/image.gif" target="_blank"><img src="https://image.jimcdn.com/app/cms/image/transf/dimension=970x10000:format=gif/path/sa16dc2497d80e05e/image/icd162bb94ffa0064/version/1551588419/image.gif" alt="image host" height="142px"/></a>
</p>

<br>

<div align="center">
<h4> | <a href="#contexto">Contexto</a> | <a href="#equipe">Equipe</a> | <a href="#desenvolvimento">Desenvolvimento</a> | <a href="#ferramentas">Ferramentas</a> | <a href="#creditos">Créditos</a> |</h4>
</div>

<p align="center">
  <img src="./src/img/fundo.png" alt="divisor" width="100%" height="5px">
</p>

<h2 id="contexto"> 🧠 CONTEXTO E OBJETIVO</h2>

<p>O projeto consiste no desenvolvimento de um sistema embarcado para controle e monitoramento do braço robótico industrial MagniBot (5/6 DOF), utilizando o microcontrolador ESP32-S3. A solução integra o acionamento dos servomotores por sinais PWM, sensores para detecção de objetos e monitoramento da força de preensão, além de sinalização visual do estado operacional por LED RGB WS2812.

O sistema implementa um ciclo operacional automatizado composto por quatro estados: inatividade (IDLE), varredura para localização de objetos, conclusão da varredura e deslocamento para captura, transporte e depósito. O controle das operações é realizado por meio de um botão físico multifuncional, com suporte a acionamentos simples, duplos e longos, além de um botão de emergência com acionamento por interrupção de hardware.

A instrumentação utiliza o sensor de distância ToF VL53L0X para detecção e localização de objetos e o sensor de corrente INA219 para monitoramento da atuação da garra e estimativa da força de preensão.

Como complemento, o projeto prevê uma interface desktop integrada ao sistema via UART/USB-CDC, responsável por apresentar a telemetria em tempo real e um modelo tridimensional animado do braço robótico, reproduzindo suas articulações com base nos ângulos teóricos derivados dos sinais PWM. A interface também disponibiliza comandos virtuais para interação com o sistema.

O objetivo é desenvolver uma solução embarcada integrada, capaz de controlar a movimentação do braço robótico, monitorar suas condições operacionais, oferecer mecanismos de segurança e demonstrar seu comportamento por meio de uma representação digital dinâmica.</p>

<p align="center">
  <img src="./src/img/fundo.png" alt="divisor" width="100%" height="5px">
</p>

<h2 id="equipe"> 👥 EQUIPE</h2>

* **Aluna/dev:** <a href="">Agatha Soares Neves</a>
* **Aluna/dev:** <a href="http://lattes.cnpq.br/2405746986360435">Anna Lígia Alves Nogueira</a>
* **Aluno/dev:** <a href="">Diogo Roberto Santos Lira</a>
* **Aluno/dev:** <a href="">Edriel Jose Pacifico Gama</a>

* **Prof. Dr. responsável:** <a href="https://www.escavador.com/sobre/1746665/alexandre-sales-vasconcelos">Alexandre Sales Vasconcelos</a>

<p align="center">
  <img src="./src/img/fundo.png" alt="divisor" width="100%" height="5px">
</p>

<h2 id="etapas"> 📑 ETAPAS DE DESENVOLVIMENTO</h2>

### Etapa 1: Diagramas em Bloco e Esquemático 

- ⚙️ **Diagrama em Blocos:** [`Diagrama em Blocos`](./doc)
- 💻 **Diagrama Esquemático:** [`Diagrama Esquemático`](./doc)


<p align="center">
  <img src="./src/img/fundo.png" alt="divisor" width="100%" height="5px">
</p>

<h2 id="ferramentas"> 🛠️ FERRAMENTAS E TECNOLOGIAS UTILIZADAS</h2>

- **Microcontrolador:** ESP32-S3 (ESP-IDF)
- **Linguagem:** C
- **Comunicação:** UART / USB-CDC
- **Atuação:** Servomotores MG996R e SG90 (PWM)
- **Sensores:** VL53L0X (ToF) / INA219 (Corrente)
- **Sinalização:** LED RGB WS2812
- **Simulador:** Wokwi Simulator
- **IDE e Desenvolvimento:** VS Code
- **Documentação e Diagramas:** PlantUML / Excalidraw
- **Modelagem 3D e Interface:** Modelo tridimensional do MagniBot e interface desktop
- **Versionamento:** Git / GitHub

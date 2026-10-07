using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using MIM.Services;
using MIM.Models;
using System.IO;
using System.Net.Sockets; // DODANO: Obsługa TCP
using System.Text;

namespace MIM
{
    public class MainPageViewModel : INotifyPropertyChanged
    {
        // --- Pola prywatne ---
        private double _motorSpeedFL;
        private double _motorSpeedFR;
        private double _motorSpeedRL;
        private double _motorSpeedRR;
        private double _linearSpeed;
        private double _rotationalSpeed;
        private string _videoStreamStatus;
        private string _ipAddress; 
        private int _tcpPort = 8080; 
        private bool _isConnected;
        private Color _connectionStatusColor;
        private string _connectionStatusText;
        private readonly IGamepadService _gamepadService;
        private double _sensorDistance1;
        private double _sensorDistance2;
        private double _internalTemp;
        private double _STMTemp;
        private double _ESPTemp;
        private CancellationTokenSource _readCancellationTokenSource;

        private byte[] _lastSentPacket = new byte[4];

        private TcpClient _tcpClient;
        private NetworkStream _tcpStream;
        private StreamReader _tcpReader;

        private string _manualCommand;
        public string ManualCommand
        {
            get => _manualCommand;
            set { _manualCommand = value; OnPropertyChanged(); }
        }

        public string IpAddress
        {
            get => _ipAddress;
            set
            {
                _ipAddress = value;
                OnPropertyChanged();
                ((Command)ConnectCommand).ChangeCanExecute();
            }
        }

        public int TcpPort
        {
            get => _tcpPort;
            set { _tcpPort = value; OnPropertyChanged(); }
        }

        public ICommand SendManualCommand => new Command<string>(async (commandParameter) =>
        {
            string targetCommand = !string.IsNullOrWhiteSpace(commandParameter) ? commandParameter : ManualCommand;

            if (!string.IsNullOrWhiteSpace(targetCommand))
            {
                byte[] binaryPacket = CreateMovementPacket(targetCommand);
                await SendRoverCommand(binaryPacket);

                if (string.IsNullOrWhiteSpace(commandParameter))
                {
                    ManualCommand = string.Empty;
                }
            }
        });

        public bool IsNotConnected => !IsConnected;

        // --- Właściwości dla silników i czujników ---
        public double MotorSpeedFL
        {
            get => _motorSpeedFL;
            set { _motorSpeedFL = value; OnPropertyChanged(); }
        }
        public double MotorSpeedFR
        {
            get => _motorSpeedFR;
            set { _motorSpeedFR = value; OnPropertyChanged(); }
        }
        public double MotorSpeedRL
        {
            get => _motorSpeedRL;
            set { _motorSpeedRL = value; OnPropertyChanged(); }
        }
        public double MotorSpeedRR
        {
            get => _motorSpeedRR;
            set { _motorSpeedRR = value; OnPropertyChanged(); }
        }
        public double SensorDistance1
        {
            get => _sensorDistance1;
            set { _sensorDistance1 = value; OnPropertyChanged(); }
        }
        public double SensorDistance2
        {
            get => _sensorDistance2;
            set { _sensorDistance2 = value; OnPropertyChanged(); }
        }
        public double LinearSpeed {
            get => _linearSpeed;
            set { _linearSpeed = value; OnPropertyChanged(); }
        }
        public double RotationalSpeed {
            get => _rotationalSpeed;
            set { _rotationalSpeed = value; OnPropertyChanged(); } 
        }
        public double InternalTemp
        {
            get => _internalTemp;
            set { _internalTemp = value; OnPropertyChanged(); }
        }
        public double STMTemp
        {
            get => _STMTemp;
            set { _STMTemp = value;OnPropertyChanged(); }
        }
        public double ESPTemp
        {
            get => _ESPTemp;
            set { _ESPTemp = value; OnPropertyChanged(); }
        }

        public string VideoStreamStatus
        {
            get => _videoStreamStatus;
            set { _videoStreamStatus = value; OnPropertyChanged(); }
        }

        public bool IsConnected
        {
            get => _isConnected;
            set
            {
                _isConnected = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(IsNotConnected));
                ((Command)ConnectCommand).ChangeCanExecute();
                ((Command)DisconnectCommand).ChangeCanExecute();
            }
        }

        public Color ConnectionStatusColor
        {
            get => _connectionStatusColor;
            set { _connectionStatusColor = value; OnPropertyChanged(); }
        }

        public string ConnectionStatusText
        {
            get => _connectionStatusText;
            set { _connectionStatusText = value; OnPropertyChanged(); }
        }

        // --- Komendy ---
        public ICommand ConnectCommand { get; }
        public ICommand DisconnectCommand { get; }

        public MainPageViewModel()
        {
            ConnectCommand = new Command(ExecuteConnect, () => !IsConnected && !string.IsNullOrEmpty(IpAddress));
            DisconnectCommand = new Command(ExecuteDisconnect, () => IsConnected);
            IpAddress = "192.168.137.102";
            VideoStreamStatus = "Oczekiwanie na strumień wideo... (00:00:00)";
            ConnectionStatusColor = Colors.Red;
            ConnectionStatusText = "Rozłączono";
          

            MotorSpeedFL = 0;
            MotorSpeedFR = 0;
            MotorSpeedRL = 0;
            MotorSpeedRR = 0;
            LinearSpeed = 0;
            RotationalSpeed = 0;
            

            _gamepadService = new MIM.Services.WindowsGamepadService();
            _gamepadService.GamepadStateChanged += OnGamepadStateChanged;
            _gamepadService.Start();
        }

        private async void ExecuteConnect()
        {
            if (string.IsNullOrEmpty(IpAddress)) return;

            try
            {
                ConnectionStatusText = $"Łączenie z {IpAddress}:{TcpPort}...";
                ConnectionStatusColor = Colors.Orange;

                _tcpClient = new TcpClient();

                await _tcpClient.ConnectAsync(IpAddress, TcpPort);

                _tcpStream = _tcpClient.GetStream();
                _tcpReader = new StreamReader(_tcpStream, Encoding.UTF8);

                IsConnected = true;
                ConnectionStatusText = "POŁĄCZONO (TCP)";
                ConnectionStatusColor = Colors.Green;

                _readCancellationTokenSource = new CancellationTokenSource();
                _ = Task.Run(() => StartListeningAsync(_readCancellationTokenSource.Token));
            }
            catch (Exception ex)
            {
                ConnectionStatusText = "Błąd połączenia TCP";
                ConnectionStatusColor = Colors.Red;
                System.Diagnostics.Debug.WriteLine($"Błąd TCP: {ex.Message}");
                ExecuteDisconnect();
            }
        }

        private void ExecuteDisconnect()
        {
            _readCancellationTokenSource?.Cancel();
            _readCancellationTokenSource?.Dispose();
            _readCancellationTokenSource = null;

            _tcpReader?.Dispose();
            _tcpReader = null;
            _tcpStream?.Dispose();
            _tcpStream = null;
            _tcpClient?.Close();
            _tcpClient?.Dispose();
            _tcpClient = null;

            IsConnected = false;
            ConnectionStatusText = "Rozłączono";
            ConnectionStatusColor = Colors.Red;
            VideoStreamStatus = "Utracono połączenie wideo.";
            _gamepadService.Stop();

            SensorDistance1 = 0;
            SensorDistance2 = 0;
            MotorSpeedFL = 0;
            MotorSpeedFR = 0;
            MotorSpeedRL = 0;
            MotorSpeedRR = 0;
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        public async Task SendRoverCommand(byte[] packet)
        {
            if (_tcpStream == null || !IsConnected || !_tcpStream.CanWrite)
                return;

            try
            { 
                await _tcpStream.WriteAsync(packet, 0, packet.Length);
                await _tcpStream.FlushAsync();
                System.Diagnostics.Debug.WriteLine($"Wysłano przez TCP: {BitConverter.ToString(packet)}");
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Błąd wysyłania TCP: {ex.Message}");
                MainThread.BeginInvokeOnMainThread(() => ExecuteDisconnect());
            }
        }

        private byte EncodeMotorByte(byte motorId, bool isReverse, byte speedRaw)
        {
            byte cleanId = (byte)(motorId & 0x03);       // max 2 bity (0-3)
            byte cleanDir = (byte)(isReverse ? 1 : 0);   // max 1 bit  (0-1)
            byte cleanSpeed = (byte)(speedRaw & 0x1F);   // max 5 bitów (0-31)

            int encoded = (cleanId << 6) | (cleanDir << 5) | cleanSpeed;
            return (byte)encoded;
        }

        private byte[] CreateMovementPacket(string command)
        {
            byte[] packet = new byte[4];
            bool isReverse = false;
            byte speed = 0;

            switch (command.ToUpper().Trim())
            {
                case "W": isReverse = false; speed = 5; break;
                case "S": isReverse = true; speed = 5; break;
                case "A":
                    packet[0] = EncodeMotorByte(0, true, 3);
                    packet[1] = EncodeMotorByte(2, false, 3);
                    packet[2] = EncodeMotorByte(1, true, 3);
                    packet[3] = EncodeMotorByte(3, false, 3);
                    return packet;
                case "D":
                    packet[0] = EncodeMotorByte(0, false, 3);
                    packet[1] = EncodeMotorByte(2, true, 3);
                    packet[2] = EncodeMotorByte(1, false, 3);
                    packet[3] = EncodeMotorByte(3, true, 3);
                    return packet;
                case "STOP":
                default:
                    isReverse = false; speed = 0; break;
            }

            packet[0] = EncodeMotorByte(0, isReverse, speed);
            packet[1] = EncodeMotorByte(1, isReverse, speed);
            packet[2] = EncodeMotorByte(2, isReverse, speed);
            packet[3] = EncodeMotorByte(3, isReverse, speed);
            return packet;
        }

        private async void OnGamepadStateChanged(object sender, GamepadState e)
        {
            if (!IsConnected || !e.IsConnected)
                return;

            double leftValue = e.LeftStickY;
            double rightValue = e.RightStickY;

            byte[] gamepadPacket = new byte[4];

            // Lewa strona (Silniki 0 i 2)
            bool leftReverse = leftValue < 0;
            byte leftSpeed = (byte)Math.Min(31, Math.Abs(leftValue) * 31.0 / 100.0);
            gamepadPacket[0] = EncodeMotorByte(0, leftReverse, leftSpeed);
            gamepadPacket[2] = EncodeMotorByte(1, leftReverse, leftSpeed);

            // Prawa strona (Silniki 1 i 3)
            bool rightReverse = rightValue < 0;
            byte rightSpeed = (byte)Math.Min(31, Math.Abs(rightValue) * 31.0 / 100.0);
            gamepadPacket[1] = EncodeMotorByte(2, rightReverse, rightSpeed);
            gamepadPacket[3] = EncodeMotorByte(3, rightReverse, rightSpeed);

            if (gamepadPacket[0] != _lastSentPacket[0] || gamepadPacket[1] != _lastSentPacket[1] ||
                gamepadPacket[2] != _lastSentPacket[2] || gamepadPacket[3] != _lastSentPacket[3])
            {
                _lastSentPacket[0] = gamepadPacket[0];
                _lastSentPacket[1] = gamepadPacket[1];
                _lastSentPacket[2] = gamepadPacket[2];
                _lastSentPacket[3] = gamepadPacket[3];


                await SendRoverCommand(gamepadPacket);
            }
        }

        private async Task StartListeningAsync(CancellationToken token)
        {
            if (_tcpReader == null || _tcpStream == null) return;

            try
            {
                while (!token.IsCancellationRequested && IsConnected && _tcpClient.Connected)
                {

                    string rxLine = await _tcpReader.ReadLineAsync(token);

                    if (rxLine == null)
                    {
                        System.Diagnostics.Debug.WriteLine("Serwer zamknął połączenie TCP.");
                        break;
                    }

                    string cleanJson = rxLine.Trim();
                    if (!string.IsNullOrWhiteSpace(cleanJson) && cleanJson.StartsWith("{") && cleanJson.EndsWith("}"))
                    {
                        try
                        {
                            var telemetry = System.Text.Json.JsonSerializer.Deserialize<TelemetryData>(cleanJson);

                            if (telemetry != null)
                            {
                                MainThread.BeginInvokeOnMainThread(() =>
                                {
                                    double ratio = (Math.PI * 0.08)/60;
                                    MotorSpeedFL = telemetry.s3*ratio;
                                    MotorSpeedFR = telemetry.s2*ratio;
                                    MotorSpeedRL = telemetry.s4*ratio;
                                    MotorSpeedRR = telemetry.s1 * ratio;

                                    SensorDistance1 = telemetry.d1;
                                    SensorDistance2 = telemetry.d2;

                                    InternalTemp = telemetry.t1;
                                    STMTemp = telemetry.t2;
                                    ESPTemp = telemetry.t3;
                                    
                                    double Vl = (MotorSpeedFL + MotorSpeedRL) / 2;
                                    double Vr = (MotorSpeedFR + MotorSpeedRR) / 2;
                                    LinearSpeed = (Vl + Vr) / 2;
                                    RotationalSpeed=(Vr - Vl) / 0.205;

                                });
                            }
                        }
                        catch (System.Text.Json.JsonException)
                        {
                            System.Diagnostics.Debug.WriteLine($"Błąd parsowania JSON: {cleanJson}");
                        }
                    }
                }
            }
            catch (OperationCanceledException)
            {
                System.Diagnostics.Debug.WriteLine("Nasłuchiwanie TCP zostało przerwane przez żądanie.");
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Błąd pętli odbiorczej TCP: {ex.Message}");
            }
            finally
            {
                if (IsConnected)
                {
                    MainThread.BeginInvokeOnMainThread(() => ExecuteDisconnect());
                }
            }
        }
    }
}